#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

struct ApiSettings
{
    bool enabled = false;
    std::wstring ip = L"127.0.0.1";
    int port = 49152;

    bool operator==(const ApiSettings &) const = default;
    static bool IsValidIp(const std::wstring &value)
    {
        IN_ADDR address4{};
        IN6_ADDR address6{};
        return InetPtonW(AF_INET, value.c_str(), &address4) == 1 || InetPtonW(AF_INET6, value.c_str(), &address6) == 1;
    }
    bool IsValid() const
    {
        return port >= 1 && port <= 65535 && IsValidIp(ip);
    }
    std::wstring HttpUrl() const
    {
        // Wildcards are bind addresses, not addresses a browser can connect to.
        auto host = ip == L"0.0.0.0" ? L"127.0.0.1" : (ip == L"::" ? L"::1" : ip);
        if (host.find(L':') != std::wstring::npos)
            host = L"[" + host + L"]";
        return L"http://" + host + L":" + std::to_wstring(port) + L"/";
    }
};
