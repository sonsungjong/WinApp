#pragma once

#include "framework.h"

namespace FileDialogs
{
    inline std::vector<std::wstring> OpenVideoFiles(HWND owner)
    {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(dialog.GetAddressOf())))) return {};

        DWORD options = 0;
        if (FAILED(dialog->GetOptions(&options)) ||
            FAILED(dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST |
                FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR))) return {};

        const COMDLG_FILTERSPEC filters[] = {
            { L"동영상 파일", L"*.mp4;*.mkv;*.avi;*.wmv;*.mov;*.webm;*.flv;*.ts;*.m4v" },
            { L"모든 파일", L"*.*" }
        };
        if (FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters)) ||
            FAILED(dialog->Show(owner))) return {};

        Microsoft::WRL::ComPtr<IShellItemArray> results;
        if (FAILED(dialog->GetResults(results.GetAddressOf()))) return {};
        DWORD count = 0;
        if (FAILED(results->GetCount(&count))) return {};

        std::vector<std::wstring> files;
        files.reserve(count);
        for (DWORD i = 0; i < count; ++i)
        {
            Microsoft::WRL::ComPtr<IShellItem> item;
            PWSTR path = nullptr;
            if (FAILED(results->GetItemAt(i, item.GetAddressOf())) ||
                FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return {};
            std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> ownedPath(path, CoTaskMemFree);
            files.emplace_back(path);
        }
        return files;
    }
}
