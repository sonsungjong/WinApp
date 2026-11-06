#pragma once

#include "framework.h"

class IMainView
{
public:
    virtual ~IMainView() = default;

    virtual void Show(int cmdShow) = 0;
    virtual HINSTANCE GetInstance() const = 0;
    virtual HWND GetHwnd() const = 0;
    virtual void SetWindowTitle(const std::wstring& title) = 0;
    virtual void UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost) = 0;
    virtual void SetTopMost(bool topMost) = 0;
    virtual void ToggleFullscreen() = 0;
    virtual void ExitFullscreen() = 0;
    virtual bool IsFullscreen() const = 0;
    virtual void SetPlayerHwnd(HWND playerHwnd) = 0;

    virtual void SetOnTimer(std::function<void(UINT_PTR)> handler) = 0;
    virtual void SetOnCommand(std::function<void(int)> handler) = 0;
    virtual void SetOnKeyDown(std::function<void(WPARAM)> handler) = 0;
    virtual void SetOnMouseWheel(std::function<void(int)> handler) = 0;
    virtual void SetOnFilesDropped(std::function<void(HDROP)> handler) = 0;
    virtual void SetOnFullscreenChanged(std::function<void(bool)> handler) = 0;
};