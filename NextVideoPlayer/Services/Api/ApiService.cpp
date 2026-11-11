#include "Services/Api/ApiService.h"
#include "Services/Api/ApiProtocol.h"
#include "Services/Api/TextEncoding.h"

ApiService::ApiService(UiDispatcher &dispatcher, Execute execute, Capture capture)
    : m_dispatcher(dispatcher), m_execute(std::move(execute)), m_capture(std::move(capture))
{
    m_server.SetHandler([this](auto id, const auto &text) { return OnMessage(id, text); });
}
ApiService::~ApiService()
{
    *m_alive = false;
    Stop();
}
void ApiService::Configure(const ApiSettings &settings)
{
    if (settings == m_settings)
        return;
    // Finish the old receiver before advancing the generation. A command still
    // being parsed on that thread must never acquire the new server's epoch.
    m_server.Stop();
    ++m_epoch;
    m_scheduled.clear();
    m_settings = settings;
    m_server.Start(settings); // Transport reports bind failures through its status.
}
void ApiService::Stop()
{
    m_server.Stop();
    ++m_epoch;
    m_scheduled.clear();
}
std::optional<bool> ApiService::OnMessage(ApiServer::ConnectionId connection, const std::string &text)
{
    Api::Command command;
    std::string error;
    if (!Api::Parse(text, command, error))
    {
        m_server.Send(connection, Api::Response(command.requestId, {false, false, "invalid_request", error}));
        return {};
    }
    const auto subscription = command.subscribe;
    const auto epoch = m_epoch.load();
    const auto alive = m_alive;
    const auto requestId = command.requestId;
    if (!m_dispatcher.Post([this, alive, epoch, connection, command = std::move(command)] {
            if (!*alive || epoch != m_epoch.load())
                return;
            try
            {
                Apply(connection, command);
            }
            catch (const std::exception &failure)
            {
                Reply(connection, command, {false, false, "execution_failed", failure.what()});
            }
        }))
        m_server.Send(connection,
                      Api::Response(requestId, {false, false, "busy", "The command queue is full; try again later"}));
    return subscription;
}
void ApiService::Reply(ApiServer::ConnectionId connection, const Api::Command &command, const Api::Result &result)
{
    const auto state = m_capture(true);
    auto response = Api::Response(command.requestId, result, &state);
    if (response.size() > Api::MaxMessageBytes)
        response = Api::Response(command.requestId,
                                 {false, false, "response_too_large", "Playlist is too large for one response"});
    m_server.Send(connection, std::move(response));
}
void ApiService::Apply(ApiServer::ConnectionId connection, const Api::Command &command)
{
    if (command.executeAtMs && command.HasChanges())
    {
        const auto delay = *command.executeAtMs - Api::UnixMilliseconds();
        if (delay > 30000 || delay < -500)
        {
            Reply(connection, command,
                  {false, false, "invalid_time", "executeAtMs must be within the next 30 seconds"});
            return;
        }
        if (delay > 0)
        {
            if (m_scheduled.size() >= 64)
            {
                Reply(connection, command, {false, false, "busy", "Scheduled command queue is full"});
                return;
            }
            auto pending = command;
            pending.executeAtMs.reset();
            m_scheduled.push_back({connection, std::move(pending), GetTickCount64() + static_cast<ULONGLONG>(delay)});
            Reply(connection, command, {true, true, {}, {}});
            return;
        }
    }
    Reply(connection, command, m_execute(command));
}
void ApiService::Tick()
{
    const auto now = GetTickCount64();
    for (std::size_t i = 0; i < m_scheduled.size();)
    {
        if (m_scheduled[i].deadline > now)
        {
            ++i;
            continue;
        }
        auto scheduled = std::move(m_scheduled[i]);
        m_scheduled.erase(m_scheduled.begin() + i);
        try
        {
            Apply(scheduled.connection, scheduled.command);
        }
        catch (const std::exception &failure)
        {
            Reply(scheduled.connection, scheduled.command, {false, false, "execution_failed", failure.what()});
        }
    }
    if (m_server.IsListening())
        m_server.Publish(Api::StateEvent(m_capture(false)));
    if (m_openPageDeadline)
    {
        if (m_server.IsListening())
        {
            ShellExecuteW(nullptr, L"open", m_settings.HttpUrl().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            m_openPageDeadline = 0;
        }
        else if (now >= m_openPageDeadline)
            m_openPageDeadline = 0;
    }
}
void ApiService::OpenControlPage()
{
    if (!m_settings.enabled)
        return;
    if (!m_server.IsListening())
        m_server.Start(m_settings);
    m_openPageDeadline = GetTickCount64() + 5000;
}
