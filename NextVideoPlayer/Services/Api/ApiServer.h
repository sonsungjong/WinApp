#pragma once
#include "Models/ApiSettings.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

class ApiServer
{
  public:
    using ConnectionId = std::uint64_t;
    using MessageHandler = std::function<std::optional<bool>(ConnectionId, const std::string &)>;
    ApiServer();
    ~ApiServer();
    ApiServer(const ApiServer &) = delete;
    ApiServer &operator=(const ApiServer &) = delete;
    void SetHandler(MessageHandler);
    void Start(const ApiSettings &);
    void Stop();
    void Send(ConnectionId, std::string);
    void Publish(std::string);
    std::wstring Status() const;
    bool IsListening() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
