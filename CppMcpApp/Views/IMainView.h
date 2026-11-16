#pragma once

#include <functional>
#include <string>

// The presenter interacts with the window through this interface only.
class IMainView
{
public:
    virtual ~IMainView() = default;

    virtual void SetWindowTitle(const std::wstring& title) = 0;
    virtual void ShowAbout() = 0;
    virtual void Close() = 0;
    virtual void SetOnCommand(std::function<void(int)> handler) = 0;
};
