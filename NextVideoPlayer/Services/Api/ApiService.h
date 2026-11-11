#pragma once
#include "Services/Api/ApiServer.h"
#include "Services/Api/ApiTypes.h"
#include "Services/Api/UiDispatcher.h"
#include <atomic>

class ApiService
{
  public:
    using Execute = std::function<Api::Result(const Api::Command &)>;
    using Capture = std::function<Api::State(bool)>;
    ApiService(UiDispatcher &, Execute, Capture);
    ~ApiService();
    void Configure(const ApiSettings &);
    void Tick(); // UI thread, after playback events have been processed
    void Stop();
    void OpenControlPage(); // Explicit user action only; waits asynchronously for startup.
    std::wstring Status() const
    {
        return m_server.Status();
    }

  private:
    struct Scheduled
    {
        ApiServer::ConnectionId connection;
        Api::Command command;
        ULONGLONG deadline;
    };
    std::optional<bool> OnMessage(ApiServer::ConnectionId, const std::string &);
    void Apply(ApiServer::ConnectionId, const Api::Command &);
    void Reply(ApiServer::ConnectionId, const Api::Command &, const Api::Result &);
    UiDispatcher &m_dispatcher;
    Execute m_execute;
    Capture m_capture;
    ApiServer m_server;
    ApiSettings m_settings;
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
    std::atomic<std::uint64_t> m_epoch{0};
    std::vector<Scheduled> m_scheduled;
    ULONGLONG m_openPageDeadline = 0;
};
