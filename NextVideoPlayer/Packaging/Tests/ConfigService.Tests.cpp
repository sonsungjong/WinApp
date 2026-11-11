#include "Models/ConfigService.h"
#include "Resource.h"
#include "mpv/client.h"
#include <shlobj.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void TestConfiguration(const fs::path& root)
{
    ConfigService defaults;
    PWSTR localPath = nullptr;
    Require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localPath)), "LocalAppData lookup");
    const fs::path expected = fs::path(localPath) / L"NextVideoPlayer" / L"config.ini";
    CoTaskMemFree(localPath);
    Require(fs::path(defaults.GetConfigPath()) == expected, "Unpackaged settings must use per-user AppData");

    ConfigService legacy((root / L"legacy" / L"config.ini").wstring());
    AppConfig original{};
    original.repeatEnabled = true;
    original.seekStepSeconds = 47;
    original.volume = 0;
    original.speed = 0.20;
    original.topMost = true;
    original.subtitleVisible = false;
    original.subtitleScale = 2.75;
    original.api = {true, L"127.0.0.1", 49200};
    legacy.Save(original);
    ConfigService current((root / L"new" / L"config.ini").wstring(), legacy.GetConfigPath());
    const AppConfig migrated = current.Load();
    Require(migrated.repeatEnabled && migrated.seekStepSeconds == 47 && migrated.volume == 0 &&
                std::abs(migrated.speed - 0.20) < 1e-8 && migrated.topMost && !migrated.subtitleVisible &&
                std::abs(migrated.subtitleScale - 2.75) < 1e-8 && migrated.api == original.api,
            "Legacy settings migration");
    AppConfig changed = migrated;
    changed.speed = 1.30;
    changed.volume = 77;
    changed.muted = true;
    changed.api = {false, L"::1", 65535};
    current.Save(changed);
    Require(current.Load().volume == 77 && current.Load().muted && std::abs(current.Load().speed - 1.30) < 1e-8 &&
                current.Load().api == changed.api,
            "Settings roundtrip and no repeated migration");
    Require(legacy.Load().volume == 0 && std::abs(legacy.Load().speed - 0.20) < 1e-8,
        "Legacy file must not be changed");

    ConfigService empty((root / L"empty" / L"config.ini").wstring());
    const AppConfig fresh = empty.Load();
    Require(!fresh.repeatEnabled && fresh.volume == 100 && fresh.speed == 1.0 && fresh.subtitleVisible,
        "First-run defaults");
    Require(!fresh.api.enabled && fresh.api.ip == L"127.0.0.1" && fresh.api.port == 49152,
            "API defaults must be disabled and loopback-only");
    Require(ApiSettings::IsValidIp(L"127.0.0.1") && ApiSettings::IsValidIp(L"::1") &&
                !ApiSettings::IsValidIp(L"not-an-IP") && !ApiSettings::IsValidIp(L""),
            "API IP validation");
    Require(ApiSettings{true, L"0.0.0.0", 49152}.HttpUrl() == L"http://127.0.0.1:49152/" &&
                ApiSettings{true, L"::", 49152}.HttpUrl() == L"http://[::1]:49152/",
            "API wildcard control page URL");
    WritePrivateProfileStringW(L"Playback", L"Volume", L"999", empty.GetConfigPath().c_str());
    WritePrivateProfileStringW(L"Playback", L"SeekStepSeconds", L"0", empty.GetConfigPath().c_str());
    WritePrivateProfileStringW(L"Playback", L"Speed", L"nan", empty.GetConfigPath().c_str());
    Require(empty.Load().volume == 100 && empty.Load().seekStepSeconds == 1 && empty.Load().speed == 1.0,
        "Invalid stored settings");
    for (const auto* port : {L"0", L"65536", L"-1", L"49152x", L""})
    {
        WritePrivateProfileStringW(L"API", L"Port", port, empty.GetConfigPath().c_str());
        Require(!empty.Load().api.IsValid(), "Invalid API port must remain invalid instead of binding another port");
    }

    const auto concurrentPath = (root / L"concurrent" / L"config.ini").wstring();
    std::exception_ptr firstError, secondError;
    std::thread first([&] { try { ConfigService(concurrentPath, legacy.GetConfigPath()).Load(); } catch (...) { firstError = std::current_exception(); } });
    std::thread second([&] { try { ConfigService(concurrentPath, legacy.GetConfigPath()).Load(); } catch (...) { secondError = std::current_exception(); } });
    first.join(); second.join();
    if (firstError) std::rethrow_exception(firstError);
    if (secondError) std::rethrow_exception(secondError);
    Require(ConfigService(concurrentPath).Load().seekStepSeconds == 47, "Concurrent first-run migration");
    std::puts("PASS: per-user settings, legacy migration, no overwrite, roundtrip, defaults, invalid input, concurrent startup.");
}

void TestPayload(const fs::path& payload)
{
    std::vector<HMODULE> modules;
    try
    {
        const HMODULE appImage = LoadLibraryExW((payload / L"NextVideoPlayer.exe").c_str(), nullptr,
                                                LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        Require(appImage != nullptr, "App resources must load without launching the player");
        modules.push_back(appImage);
        const HRSRC information = FindResourceW(appImage, MAKEINTRESOURCEW(IDR_APP_INFO), RT_RCDATA);
        const HGLOBAL loaded = information ? LoadResource(appImage, information) : nullptr;
        const auto* data = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
        Require(data != nullptr, "Embedded app information is missing");
        const std::string embedded(data, SizeofResource(appImage, information));
        std::ifstream input(payload / L"AppInfo.json", std::ios::binary);
        const std::string packaged(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>{});
        Require(!packaged.empty() && embedded == packaged, "Packaged app information must match the executable");
        for (const int id : {IDR_OSS_LICENSE, IDR_CJSON_LICENSE, IDR_LWS_LICENSE, IDR_LWS_NOTICE, IDR_FONT_LICENSE,
                            IDR_FFMPEG_LGPL_LICENSE, IDR_GPL3_LICENSE})
            Require(FindResourceW(appImage, MAKEINTRESOURCEW(id), RT_RCDATA) != nullptr,
                    "Embedded license text is missing");
        for (const auto& file : fs::directory_iterator(payload))
        {
            if (file.path().extension() != L".dll") continue;
            const HMODULE module = LoadLibraryExW(file.path().c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module) throw std::system_error(GetLastError(), std::system_category(), file.path().string());
            modules.push_back(module);
            wchar_t actualPath[32768]{};
            Require(GetModuleFileNameW(module, actualPath, 32768) != 0 && fs::equivalent(file.path(), actualPath),
                "Runtime must load from the package, not the development machine");
            if (file.path().filename() == L"libmpv-2.dll")
            {
                const auto apiVersion = reinterpret_cast<decltype(&mpv_client_api_version)>(GetProcAddress(module, "mpv_client_api_version"));
                Require(apiVersion && apiVersion() != 0, "Bundled libmpv client API");
            }
        }
        Require(modules.size() >= 6, "Bundled DLLs are missing");
    }
    catch (...)
    {
        for (auto it = modules.rbegin(); it != modules.rend(); ++it) FreeLibrary(*it);
        throw;
    }
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) FreeLibrary(*it);
    std::puts("PASS: packaged runtimes and libmpv load successfully. No player window or playback started.");
    std::puts("PASS: packaged app information matches the embedded data, and license resources are present.");
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Require(argc == 3, "Usage: ConfigService.Tests <scratch-directory> <payload-directory>");
        TestConfiguration(fs::absolute(argv[1]));
        TestPayload(fs::absolute(argv[2]));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
