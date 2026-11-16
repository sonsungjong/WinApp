#pragma once

#include <string>

// Shell state only (for example, the application title). Home state belongs in HomeModel.
class MainModel
{
public:
    explicit MainModel(std::wstring windowTitle);

    const std::wstring& GetWindowTitle() const;

private:
    std::wstring m_windowTitle;
};
