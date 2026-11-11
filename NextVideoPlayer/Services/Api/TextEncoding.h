#pragma once
#include "framework.h"
#include <cstdint>
#include <stdexcept>

namespace Api
{
inline std::string ToUtf8(const std::wstring &text)
{
    if (text.empty())
        return {};
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (!count)
        throw std::invalid_argument("Invalid Unicode text");
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count,
                        nullptr, nullptr);
    return result;
}
inline std::wstring ToWide(const std::string &text)
{
    if (text.empty())
        return {};
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!count)
        throw std::invalid_argument("Invalid UTF-8 text");
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                        count);
    return result;
}
inline std::int64_t UnixMilliseconds()
{
    FILETIME time{};
    GetSystemTimePreciseAsFileTime(&time);
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return static_cast<std::int64_t>((value.QuadPart - 116444736000000000ULL) / 10000);
}
} // namespace Api
