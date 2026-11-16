#pragma once

#include <string>

// State for the Home screen. Screen-specific state belongs here, not in MainModel.
class HomeModel
{
public:
    const std::wstring& GetTitle() const;

private:
    std::wstring m_title = L"Home";
};
