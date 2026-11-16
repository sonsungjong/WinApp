#pragma once

#include <functional>
#include <string>

class ISettingView
{
public:
    virtual ~ISettingView() = default;

    virtual void SetTitle(const std::wstring& title) = 0;
    virtual void ShowModal() = 0;
    virtual void Close() = 0;
    virtual void SetOnCloseRequested(std::function<void()> handler) = 0;
};
