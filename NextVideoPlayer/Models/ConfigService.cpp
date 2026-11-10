#include "Models/ConfigService.h"
#include "Models/PlaybackSpeed.h"
#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <appmodel.h>
#include <shlobj.h>
#include <system_error>
#include <utility>

namespace
{
    std::filesystem::path GetSettingsDirectory()
    {
        PWSTR rawPath = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &rawPath);
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> localAppData(rawPath, &CoTaskMemFree);
        if (FAILED(result))
            throw std::system_error(static_cast<int>(result), std::system_category(), "LocalAppData");

        std::filesystem::path directory(localAppData.get());
        UINT32 length = 0;
        LONG packageResult = GetCurrentPackageFamilyName(&length, nullptr);
        if (packageResult == APPMODEL_ERROR_NO_PACKAGE)
            return directory / L"NextVideoPlayer";
        if (packageResult != ERROR_INSUFFICIENT_BUFFER)
            throw std::system_error(packageResult, std::system_category(), "Package identity");

        std::wstring familyName(length, L'\0');
        packageResult = GetCurrentPackageFamilyName(&length, familyName.data());
        if (packageResult != ERROR_SUCCESS)
            throw std::system_error(packageResult, std::system_category(), "Package identity");
        familyName.resize(length - 1);
        return directory / L"Packages" / familyName / L"LocalState";
    }

    std::wstring GetLegacyConfigPath()
    {
        std::wstring modulePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (length == 0 || length >= modulePath.size())
            throw std::system_error(GetLastError(), std::system_category(), "Executable path");
        modulePath.resize(length);
        return (std::filesystem::path(modulePath).parent_path() / L"config.ini").wstring();
    }
}

ConfigService::ConfigService()
    : ConfigService((GetSettingsDirectory() / L"config.ini").wstring(), GetLegacyConfigPath())
{
}

ConfigService::ConfigService(std::wstring configPath, std::wstring legacyConfigPath)
    : m_configPath(std::move(configPath)), m_legacyConfigPath(std::move(legacyConfigPath))
{
    if (!std::filesystem::path(m_configPath).is_absolute())
        throw std::invalid_argument("Config path must be absolute");
}

AppConfig ConfigService::Load() const
{
    AppConfig config{};

    std::filesystem::create_directories(std::filesystem::path(m_configPath).parent_path());
    if (!std::filesystem::exists(m_configPath) && !m_legacyConfigPath.empty() &&
        std::filesystem::is_regular_file(m_legacyConfigPath))
    {
        // Preserve old settings without modifying the legacy file. skip_existing
        // also handles two players starting their migration concurrently.
        std::filesystem::copy_file(m_legacyConfigPath, m_configPath,
            std::filesystem::copy_options::skip_existing);
    }

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
    if (parsedSpeed >= PlaybackSpeed::kMinimum && parsedSpeed <= PlaybackSpeed::kMaximum)
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
    std::filesystem::create_directories(std::filesystem::path(m_configPath).parent_path());
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

