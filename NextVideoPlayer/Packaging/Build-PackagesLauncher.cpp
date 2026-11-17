#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cwctype>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace
{
void Print(const std::wstring& text)
{
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    DWORD written = 0;
    if (GetConsoleMode(output, &mode))
    {
        WriteConsoleW(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        utf8.data(), size, nullptr, nullptr);
    WriteFile(output, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
}

struct EnvironmentKeyLess
{
    bool operator()(const std::wstring& left, const std::wstring& right) const
    {
        return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};

std::wstring SanitizeWindowsPowerShellModulePath(const std::wstring& value)
{
    auto containsInsensitive = [](const std::wstring& haystack, const std::wstring& needle) {
        if (needle.size() > haystack.size()) return false;
        for (size_t i = 0; i <= haystack.size() - needle.size(); ++i)
        {
            if (CompareStringOrdinal(haystack.c_str() + i, static_cast<int>(needle.size()),
                needle.c_str(), static_cast<int>(needle.size()), TRUE) == CSTR_EQUAL)
                return true;
        }
        return false;
    };

    std::wstring filtered;
    size_t start = 0;
    while (start <= value.size())
    {
        const size_t sep = value.find(L';', start);
        const std::wstring item = (sep == std::wstring::npos)
            ? value.substr(start) : value.substr(start, sep - start);
        if (!item.empty() && !containsInsensitive(item, L"\\PowerShell\\"))
        {
            if (!filtered.empty()) filtered += L';';
            filtered += item;
        }
        if (sep == std::wstring::npos) break;
        start = sep + 1;
    }
    return filtered;
}

std::wstring ChildEnvironment(bool sanitizePsModulePath)
{
    // Some launch environments contain both PATH and Path, which breaks MSBuild.
    // Also strip PowerShell 7 module paths when launching Windows PowerShell 5.1.
    // Deduplicate only the child environment; never change the user's settings.
    std::map<std::wstring, std::wstring, EnvironmentKeyLess> variables;
    LPWCH environment = GetEnvironmentStringsW();
    if (!environment)
        return {};
    for (const wchar_t* entry = environment; *entry; entry += wcslen(entry) + 1)
    {
        const std::wstring value(entry);
        const auto separator = value.find(L'=', value.front() == L'=' ? 1 : 0);
        if (separator != std::wstring::npos)
            variables[value.substr(0, separator)] = value.substr(separator + 1);
    }
    FreeEnvironmentStringsW(environment);
    if (sanitizePsModulePath)
    {
        auto it = variables.find(L"PSModulePath");
        if (it != variables.end())
            it->second = SanitizeWindowsPowerShellModulePath(it->second);
    }
    std::wstring result;
    for (const auto& [name, value] : variables)
    {
        result += name + L"=" + value;
        result += L'\0';
    }
    result += L'\0';
    return result;
}

std::filesystem::path FindPowerShell()
{
    std::vector<wchar_t> buffer(32768);
    const UINT sysLen = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (sysLen && sysLen < buffer.size())
    {
        const auto windowsPowerShell =
            std::filesystem::path(buffer.data()) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        if (std::filesystem::is_regular_file(windowsPowerShell))
            return windowsPowerShell;
    }
    const DWORD foundBuiltin = SearchPathW(nullptr, L"powershell.exe", nullptr,
        static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (foundBuiltin && foundBuiltin < buffer.size())
        return std::filesystem::path(buffer.data());

    const DWORD length = GetEnvironmentVariableW(L"ProgramFiles", buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length && length < buffer.size())
    {
        const auto installed = std::filesystem::path(buffer.data()) / L"PowerShell" / L"7" / L"pwsh.exe";
        if (std::filesystem::is_regular_file(installed))
            return installed;
    }
    const DWORD foundPwsh = SearchPathW(nullptr, L"pwsh.exe", nullptr,
        static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    return foundPwsh && foundPwsh < buffer.size() ? std::filesystem::path(buffer.data()) : std::filesystem::path();
}

int Run(bool noOpen)
{
    std::vector<wchar_t> module(32768);
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (!length || length >= module.size())
    {
        Print(L"실행파일 위치를 확인하지 못했습니다.\r\n");
        return 1;
    }
    const auto project = std::filesystem::path(module.data()).parent_path();
    const auto script = project / L"Packaging" / L"Build-Release.ps1";
    if (!std::filesystem::is_regular_file(script))
    {
        Print(L"Packaging\\Build-Release.ps1을 찾을 수 없습니다.\r\n"
            L"이 실행파일을 NextVideoPlayer 프로젝트 폴더에 두고 실행해 주세요.\r\n");
        return 1;
    }
    const auto powerShell = FindPowerShell();
    if (powerShell.empty())
    {
        Print(L"Windows PowerShell(powershell.exe)을 찾을 수 없습니다.\r\n");
        return 1;
    }
    const bool usingWindowsPowerShell =
        CompareStringOrdinal(powerShell.filename().c_str(), -1, L"powershell.exe", -1, TRUE) == CSTR_EQUAL;
    auto environment = ChildEnvironment(usingWindowsPowerShell);
    if (environment.empty())
    {
        Print(L"빌드 실행 환경을 준비하지 못했습니다.\r\n");
        return 1;
    }
    std::wstring command = L"\"" + powerShell.wstring() +
        L"\" -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() + L"\"";
    if (noOpen)
        command += L" -NoOpen";

    Print(L"NextVideoPlayer MSI / MSIX 패키지 만들기\r\n"
        L"최신 소스 빌드 → MSI·MSIX 생성 → 검증을 진행합니다.\r\n\r\n");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(powerShell.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_UNICODE_ENVIRONMENT, environment.data(), project.c_str(), &startup, &process))
    {
        Print(L"빌드를 시작하지 못했습니다. Windows 오류: " + std::to_wstring(GetLastError()) + L"\r\n");
        return 1;
    }
    CloseHandle(process.hThread);
    DWORD exitCode = 1;
    if (WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0)
        GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
}

int wmain(int argc, wchar_t* argv[])
{
    bool noOpen = false;
    bool noPause = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::wstring option(argv[index]);
        if (option == L"--no-open") noOpen = true;
        else if (option == L"--no-pause") noPause = true;
        else
        {
            Print(L"지원 옵션: --no-open, --no-pause\r\n");
            return 2;
        }
    }
    SetConsoleTitleW(L"NextVideoPlayer - MSI / MSIX 패키지 만들기");
    int exitCode = 1;
    try { exitCode = Run(noOpen); }
    catch (...) { Print(L"빌드 준비 중 오류가 발생했습니다. 프로젝트 폴더와 파일 접근 권한을 확인해 주세요.\r\n"); }
    if (exitCode != 0)
    {
        Print(L"\r\n패키지 생성에 실패했습니다. 위의 오류 내용을 확인해 주세요.\r\n");
        DWORD mode = 0;
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (!noPause && GetConsoleMode(input, &mode))
        {
            Print(L"Enter 키를 누르면 닫힙니다.\r\n");
            wchar_t buffer[2];
            DWORD read = 0;
            ReadConsoleW(input, buffer, 2, &read, nullptr);
        }
    }
    return exitCode;
}
