#include "Services/Api/ApiServer.h"
#include "Services/Api/ApiTypes.h"
#include "Services/Api/ApiControlPage.h"
#include "Services/Api/TextEncoding.h"
#pragma warning(push)
#pragma warning(disable : 4200) // Upstream C headers use flexible-array members.
#include <libwebsockets.h>
#pragma warning(pop)
#include <array>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>

struct ApiServer::Impl
{
    struct Client
    {
        ConnectionId id = 0;
        std::string incoming;
        std::deque<std::string> replies;
        std::optional<std::string> state;
        std::size_t queuedBytes = 0;
        bool subscribed = true, close = false;
    };
    struct Reply
    {
        ConnectionId id;
        std::string text;
    };
    ApiSettings settings;
    MessageHandler handler;
    std::thread thread;
    std::atomic<bool> stop{true}, listening{false};
    std::atomic<int> clientCount{0};
    mutable std::mutex statusMutex;
    std::wstring status = L"API 꺼짐";
    std::mutex contextMutex, queueMutex;
    lws_context *context = nullptr;
    std::deque<Reply> outgoing;
    std::size_t outgoingBytes = 0;
    std::optional<std::string> event;
    std::unordered_set<ConnectionId> closeIds;
    std::string latestState;
    std::unordered_map<lws *, Client> clients;
    std::unordered_map<lws *, std::size_t> httpOffsets;
    ConnectionId nextId = 1;
    std::array<lws_protocols, 2> protocols{};

    void SetStatus(std::wstring value)
    {
        std::lock_guard lock(statusMutex);
        status = std::move(value);
    }
    void Wake()
    {
        std::lock_guard lock(contextMutex);
        if (context)
            lws_cancel_service(context);
    }
    std::string Header(lws *socket, lws_token_indexes token)
    {
        std::array<char, 512> text{};
        const int length = lws_hdr_copy(socket, text.data(), static_cast<int>(text.size()), token);
        return length > 0 ? std::string(text.data(), length) : std::string{};
    }
    bool HostAllowed(const std::string &authority) const
    {
        std::string host, port;
        if (authority.empty() || authority.size() > 256)
            return false;
        if (authority[0] == '[')
        {
            const auto close = authority.find(']');
            if (close == std::string::npos)
                return false;
            host = authority.substr(1, close - 1);
            if (close + 1 < authority.size())
            {
                if (authority[close + 1] != ':')
                    return false;
                port = authority.substr(close + 2);
            }
        }
        else
        {
            const auto colon = authority.rfind(':');
            host = authority.substr(0, colon);
            if (colon != std::string::npos)
                port = authority.substr(colon + 1);
        }
        if (port.empty())
        {
            if (settings.port != 80)
                return false;
        }
        else if (port != std::to_string(settings.port))
            return false;
        return host == "localhost" || ApiSettings::IsValidIp(Api::ToWide(host));
    }
    bool OriginAllowed(lws *socket)
    {
        if (lws_hdr_total_length(socket, WSI_TOKEN_ORIGIN) >= 512)
            return false;
        const auto host = Header(socket, WSI_TOKEN_HOST);
        if (!HostAllowed(host))
            return false;
        const auto origin = Header(socket, WSI_TOKEN_ORIGIN);
        // Native clients may omit Origin. Browsers must use the built-in same-origin page.
        return origin.empty() || origin == "http://" + host;
    }
    void RequestWrite(lws *socket)
    {
        lws_callback_on_writable(socket);
    }
    void DrainOutgoing()
    {
        std::deque<Reply> replies;
        std::optional<std::string> state;
        std::unordered_set<ConnectionId> closing;
        {
            std::lock_guard lock(queueMutex);
            replies.swap(outgoing);
            outgoingBytes = 0;
            state.swap(event);
            closing.swap(closeIds);
        }
        for (auto &[socket, client] : clients)
            if (closing.contains(client.id))
            {
                client.close = true;
                lws_set_timer_usecs(socket, 1);
            }
        for (auto &reply : replies)
            for (auto &[socket, client] : clients)
                if (client.id == reply.id)
                {
                    if (client.replies.size() >= 32 ||
                        client.queuedBytes + reply.text.size() > 2 * Api::MaxMessageBytes)
                    {
                        client.close = true;
                        lws_set_timer_usecs(socket, 1);
                    }
                    else
                    {
                        client.queuedBytes += reply.text.size();
                        client.replies.push_back(std::move(reply.text));
                    }
                    RequestWrite(socket);
                    break;
                }
        if (state)
        {
            latestState = std::move(*state);
            for (auto &[socket, client] : clients)
                if (client.subscribed)
                {
                    client.state = latestState;
                    RequestWrite(socket);
                }
        }
    }
    int Callback(lws *socket, lws_callback_reasons reason, void *in, std::size_t length)
    {
        switch (reason)
        {
        case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
            return OriginAllowed(socket) ? 0 : 1;
        case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
            return clients.size() < 8 && Header(socket, WSI_TOKEN_GET_URI) == "/api/v1" && OriginAllowed(socket) ? 0
                                                                                                                 : 1;
        case LWS_CALLBACK_HTTP: {
            const std::string path(static_cast<const char *>(in), length);
            if (path != "/" || lws_hdr_total_length(socket, WSI_TOKEN_GET_URI) == 0 || httpOffsets.size() >= 8)
            {
                lws_return_http_status(socket, HTTP_STATUS_NOT_FOUND, nullptr);
                return -1;
            }
            std::array<unsigned char, LWS_PRE + 1024> headers{};
            auto *start = headers.data() + LWS_PRE;
            auto *p = start;
            auto *end = headers.data() + headers.size();
            if (lws_add_http_common_headers(socket, HTTP_STATUS_OK, "text/html; charset=utf-8", Api::ControlPage.size(),
                                            &p, end))
                return -1;
            if (lws_add_http_header_by_name(socket, reinterpret_cast<const unsigned char *>("x-frame-options:"),
                                            reinterpret_cast<const unsigned char *>("DENY"), 4, &p, end))
                return -1;
            if (lws_add_http_header_by_name(socket, reinterpret_cast<const unsigned char *>("x-content-type-options:"),
                                            reinterpret_cast<const unsigned char *>("nosniff"), 7, &p, end))
                return -1;
            if (lws_finalize_write_http_header(socket, start, &p, end))
                return -1;
            httpOffsets[socket] = 0;
            RequestWrite(socket);
            return 0;
        }
        case LWS_CALLBACK_HTTP_WRITEABLE: {
            auto found = httpOffsets.find(socket);
            if (found == httpOffsets.end())
                return 0;
            auto &offset = found->second;
            const auto count = std::min<std::size_t>(4096, Api::ControlPage.size() - offset);
            std::array<unsigned char, LWS_PRE + 4096> bytes{};
            std::memcpy(bytes.data() + LWS_PRE, Api::ControlPage.data() + offset, count);
            if (lws_write(socket, bytes.data() + LWS_PRE, count, LWS_WRITE_HTTP) < static_cast<int>(count))
                return -1;
            offset += count;
            if (offset == Api::ControlPage.size())
            {
                httpOffsets.erase(found);
                return lws_http_transaction_completed(socket) ? -1 : 0;
            }
            RequestWrite(socket);
            return 0;
        }
        case LWS_CALLBACK_ESTABLISHED: {
            Client client;
            client.id = nextId++;
            if (!latestState.empty())
                client.state = latestState;
            clients.emplace(socket, std::move(client));
            clientCount = static_cast<int>(clients.size());
            RequestWrite(socket);
            return 0;
        }
        case LWS_CALLBACK_RECEIVE: {
            auto found = clients.find(socket);
            if (found == clients.end())
                return -1;
            auto &client = found->second;
            if (client.close)
                return -1;
            if (lws_frame_is_binary(socket) || length > Api::MaxMessageBytes - client.incoming.size())
                return -1;
            if (client.incoming.empty())
                lws_set_timer_usecs(socket, 10 * LWS_US_PER_SEC);
            client.incoming.append(static_cast<const char *>(in), length);
            if (lws_is_final_fragment(socket) && !lws_remaining_packet_payload(socket))
            {
                lws_set_timer_usecs(socket, LWS_SET_TIMER_USEC_CANCEL);
                if (handler)
                {
                    auto subscription = handler(client.id, client.incoming);
                    if (subscription)
                    {
                        client.subscribed = *subscription;
                        if (!*subscription)
                            client.state.reset();
                    }
                }
                client.incoming.clear();
            }
            return 0;
        }
        case LWS_CALLBACK_TIMER:
            return -1;
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            auto found = clients.find(socket);
            if (found == clients.end())
                return 0;
            auto &client = found->second;
            if (client.close)
                return -1;
            std::string message;
            if (!client.replies.empty())
            {
                message = std::move(client.replies.front());
                client.replies.pop_front();
                client.queuedBytes -= message.size();
            }
            else if (client.state)
            {
                message = std::move(*client.state);
                client.state.reset();
            }
            else
                return 0;
            std::vector<unsigned char> bytes(LWS_PRE + message.size());
            std::memcpy(bytes.data() + LWS_PRE, message.data(), message.size());
            if (lws_write(socket, bytes.data() + LWS_PRE, message.size(), LWS_WRITE_TEXT) <
                static_cast<int>(message.size()))
                return -1;
            if (!client.replies.empty() || client.state)
                RequestWrite(socket);
            return 0;
        }
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_CLOSED_HTTP:
        case LWS_CALLBACK_WSI_DESTROY:
            clients.erase(socket);
            httpOffsets.erase(socket);
            clientCount = static_cast<int>(clients.size());
            return 0;
        case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
            DrainOutgoing();
            return 0;
        default:
            return 0;
        }
    }
    static int Trampoline(lws *socket, lws_callback_reasons reason, void *, void *in, std::size_t length) noexcept
    {
        if (!socket)
            return 0;
        auto *self = static_cast<Impl *>(lws_context_user(lws_get_context(socket)));
        if (!self)
            return 0;
        try
        {
            return self->Callback(socket, reason, in, length);
        }
        catch (const std::exception &error)
        {
            OutputDebugStringA(error.what());
            return -1;
        }
        catch (...)
        {
            return -1;
        }
    }
    void Run() noexcept
    {
        lws_context *local = nullptr;
        try
        {
            const std::string ip = Api::ToUtf8(settings.ip);
            protocols[0].name = "nextvideoplayer-v1";
            protocols[0].callback = Trampoline;
            protocols[0].rx_buffer_size = 4096;
            lws_context_creation_info info{};
            info.port = settings.port;
            info.iface = ip.c_str();
            info.protocols = protocols.data();
            info.user = this;
            info.gid = -1;
            info.uid = -1;
            info.fd_limit_per_thread = 40;
            info.max_http_header_data2 = 8192;
            info.max_http_header_pool2 = 16;
            info.timeout_secs = 10;
            lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);
            local = lws_create_context(&info);
            if (!local)
            {
                SetStatus(L"API를 시작할 수 없습니다. IP·포트를 확인하세요.");
                return;
            }
            {
                std::lock_guard lock(contextMutex);
                context = local;
            }
            listening = true;
            SetStatus(L"API 대기 중 " + settings.HttpUrl());
            while (!stop)
            {
                DrainOutgoing();
                if (lws_service(local, 50) < 0)
                {
                    SetStatus(L"API 통신이 중단되었습니다. API 설정을 다시 적용하세요.");
                    break;
                }
            }
        }
        catch (const std::exception &error)
        {
            OutputDebugStringA(error.what());
            SetStatus(L"API 오류가 발생했습니다.");
        }
        catch (...)
        {
            SetStatus(L"API 오류가 발생했습니다.");
        }
        listening = false;
        {
            std::lock_guard lock(contextMutex);
            context = nullptr;
            if (local)
                lws_context_destroy(local);
        }
        clients.clear();
        httpOffsets.clear();
        clientCount = 0;
    }
};

ApiServer::ApiServer() : m_impl(std::make_unique<Impl>())
{
}
ApiServer::~ApiServer()
{
    Stop();
}
void ApiServer::SetHandler(MessageHandler handler)
{
    m_impl->handler = std::move(handler);
}
void ApiServer::Start(const ApiSettings &settings)
{
    Stop();
    m_impl->settings = settings;
    if (!settings.enabled)
        return;
    if (!settings.IsValid())
    {
        m_impl->SetStatus(L"API의 IP와 1~65535 포트를 확인하세요.");
        return;
    }
    m_impl->stop = false;
    m_impl->SetStatus(L"API 시작 중…");
    try
    {
        m_impl->thread = std::thread([self = m_impl.get()] { self->Run(); });
    }
    catch (const std::exception &)
    {
        m_impl->stop = true;
        m_impl->SetStatus(L"API 스레드를 시작할 수 없습니다.");
    }
}
void ApiServer::Stop()
{
    m_impl->stop = true;
    m_impl->Wake();
    if (m_impl->thread.joinable())
        m_impl->thread.join();
    {
        std::lock_guard lock(m_impl->queueMutex);
        m_impl->outgoing.clear();
        m_impl->outgoingBytes = 0;
        m_impl->event.reset();
        m_impl->closeIds.clear();
    }
    m_impl->listening = false;
    m_impl->latestState.clear();
    m_impl->SetStatus(L"API 꺼짐");
}
void ApiServer::Send(ConnectionId id, std::string text)
{
    if (m_impl->stop || text.size() > Api::MaxMessageBytes)
        return;
    {
        std::lock_guard lock(m_impl->queueMutex);
        if (m_impl->outgoing.size() >= 128 || m_impl->outgoingBytes + text.size() > 2 * Api::MaxMessageBytes)
        {
            if (m_impl->closeIds.size() < 32)
                m_impl->closeIds.insert(id);
        }
        else
        {
            m_impl->outgoingBytes += text.size();
            m_impl->outgoing.push_back({id, std::move(text)});
        }
    }
    m_impl->Wake();
}
void ApiServer::Publish(std::string text)
{
    if (m_impl->stop || text.size() > Api::MaxMessageBytes)
        return;
    {
        std::lock_guard lock(m_impl->queueMutex);
        m_impl->event = std::move(text);
    }
    m_impl->Wake();
}
std::wstring ApiServer::Status() const
{
    std::lock_guard lock(m_impl->statusMutex);
    return m_impl->status +
           (m_impl->listening ? L" / 연결 " + std::to_wstring(m_impl->clientCount.load()) + L"개" : L"");
}
bool ApiServer::IsListening() const
{
    return m_impl->listening;
}
