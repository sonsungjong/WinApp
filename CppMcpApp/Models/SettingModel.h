#pragma once

#include <string>

// State for the settings popup. Add setting values here when needed.
class SettingModel
{
public:
    const std::wstring& GetTitle() const;

private:
    std::wstring m_title = L"설정";
};
