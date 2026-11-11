#pragma once

#include "framework.h"

struct AppInfo
{
    std::wstring developer;
    std::wstring email;
    std::wstring youtubeUrl;
    std::wstring displayText;

    static AppInfo Load(HINSTANCE instance);
    static std::wstring LicenseText(HINSTANCE instance);
};
