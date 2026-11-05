#include "Models/ConfigService.h"
#include <algorithm>
#include <cwchar>
#include <filesystem>

ConfigService::ConfigService()
{
    WCHAR modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();
    m_configPath = (exeDir / L"config.ini").wstring();
}

AppConfig ConfigService::Load() const
{
    AppConfig config{};

    DWORD attr = GetFileAttributesW(m_configPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        Save(config);
        return config;
    }

    config.repeatEnabled = GetPrivateProfileIntW(L"Playback", L"Repeat", 0, m_configPath.c_str()) != 0;
    config.seekStepSeconds = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Playback", L"SeekStepSeconds", 5, m_configPath.c_str())),
        1,
        600);
    config.volume = std::clamp(
        static_cast<int>(GetPrivateProfileIntW(L"Playback", L"Volume", 100, m_configPath.c_str())),
        0,
        100);
    config.muted = GetPrivateProfileIntW(L"Playback", L"Muted", 0, m_configPath.c_str()) != 0;

    WCHAR speedBuf[32]{};
    GetPrivateProfileStringW(L"Playback", L"Speed", L"1.0", speedBuf, 32, m_configPath.c_str());
    double parsedSpeed = _wtof(speedBuf);
    if (parsedSpeed >= 0.25 && parsedSpeed <= 4.0)
    {
        config.speed = parsedSpeed;
    }

    config.topMost = GetPrivateProfileIntW(L"Window", L"TopMost", 0, m_configPath.c_str()) != 0;
    config.subtitleVisible = GetPrivateProfileIntW(L"Subtitle", L"Visible", 1, m_configPath.c_str()) != 0;

    WCHAR subScaleBuf[32]{};
    GetPrivateProfileStringW(L"Subtitle", L"Scale", L"1.0", subScaleBuf, 32, m_configPath.c_str());
    double parsedScale = _wtof(subScaleBuf);
    if (parsedScale >= 0.3 && parsedScale <= 3.0)
    {
        config.subtitleScale = parsedScale;
    }

    return config;
}

void ConfigService::Save(const AppConfig& config) const
{
    WritePrivateProfileStringW(L"Playback", L"Repeat", config.repeatEnabled ? L"1" : L"0", m_configPath.c_str());
    WritePrivateProfileStringW(L"Playback", L"SeekStepSeconds", std::to_wstring(config.seekStepSeconds).c_str(), m_configPath.c_str());
    WritePrivateProfileStringW(L"Playback", L"Volume", std::to_wstring(config.volume).c_str(), m_configPath.c_str());
    WritePrivateProfileStringW(L"Playback", L"Muted", config.muted ? L"1" : L"0", m_configPath.c_str());

    WCHAR speedBuf[32]{};
    swprintf_s(speedBuf, L"%.2f", config.speed);
    WritePrivateProfileStringW(L"Playback", L"Speed", speedBuf, m_configPath.c_str());

    WritePrivateProfileStringW(L"Window", L"TopMost", config.topMost ? L"1" : L"0", m_configPath.c_str());
    WritePrivateProfileStringW(L"Subtitle", L"Visible", config.subtitleVisible ? L"1" : L"0", m_configPath.c_str());

    WCHAR scaleBuf[32]{};
    swprintf_s(scaleBuf, L"%.2f", config.subtitleScale);
    WritePrivateProfileStringW(L"Subtitle", L"Scale", scaleBuf, m_configPath.c_str());
}

const std::wstring& ConfigService::GetConfigPath() const
{
    return m_configPath;
}

