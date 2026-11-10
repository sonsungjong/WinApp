#include "Models/ConfigService.h"
#include "mpv/client.h"
#include <shlobj.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    legacy.Save(original);
    ConfigService current((root / L"new" / L"config.ini").wstring(), legacy.GetConfigPath());
    const AppConfig migrated = current.Load();
    Require(migrated.repeatEnabled && migrated.seekStepSeconds == 47 && migrated.volume == 0 &&
        std::abs(migrated.speed - 0.20) < 1e-8 && migrated.topMost && !migrated.subtitleVisible &&
        std::abs(migrated.subtitleScale - 2.75) < 1e-8, "Legacy settings migration");
    AppConfig changed = migrated;
    changed.speed = 1.30;
    changed.volume = 77;
    changed.muted = true;
    current.Save(changed);
    Require(current.Load().volume == 77 && current.Load().muted &&
        std::abs(current.Load().speed - 1.30) < 1e-8, "Settings roundtrip and no repeated migration");
    Require(legacy.Load().volume == 0 && std::abs(legacy.Load().speed - 0.20) < 1e-8,
        "Legacy file must not be changed");

    ConfigService empty((root / L"empty" / L"config.ini").wstring());
    const AppConfig fresh = empty.Load();
    Require(!fresh.repeatEnabled && fresh.volume == 100 && fresh.speed == 1.0 && fresh.subtitleVisible,
        "First-run defaults");
    WritePrivateProfileStringW(L"Playback", L"Volume", L"999", empty.GetConfigPath().c_str());
    WritePrivateProfileStringW(L"Playback", L"SeekStepSeconds", L"0", empty.GetConfigPath().c_str());
    WritePrivateProfileStringW(L"Playback", L"Speed", L"nan", empty.GetConfigPath().c_str());
    Require(empty.Load().volume == 100 && empty.Load().seekStepSeconds == 1 && empty.Load().speed == 1.0,
        "Invalid stored settings");

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
        Require(modules.size() >= 5, "Bundled DLLs are missing");
    }
    catch (...)
    {
        for (auto it = modules.rbegin(); it != modules.rend(); ++it) FreeLibrary(*it);
        throw;
    }
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) FreeLibrary(*it);
    std::puts("PASS: packaged runtimes and libmpv load successfully. No player window or playback started.");
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
