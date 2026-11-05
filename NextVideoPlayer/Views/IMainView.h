#pragma once

#include "framework.h"

class IMainView
{
public:
    virtual ~IMainView() = default;

    virtual void Show(int cmdShow) = 0;
    virtual HWND GetHwnd() const = 0;
    virtual HWND GetVideoHwnd() const = 0;
    virtual void SetVideoVisible(bool visible) = 0;
    virtual void SetWindowTitle(const std::wstring& title) = 0;
    virtual void SetStatusText(const std::wstring& statusText) = 0;
    virtual void SetRepeatChecked(bool checked) = 0;
    virtual std::wstring PromptOpenVideoFile() = 0;
    virtual void Render() = 0;

    virtual void SetOnOpenFileRequested(std::function<void()> handler) = 0;
    virtual void SetOnFileDropped(std::function<void(const std::wstring&)> handler) = 0;
    virtual void SetOnPlayPauseRequested(std::function<void()> handler) = 0;
    virtual void SetOnStopRequested(std::function<void()> handler) = 0;
    virtual void SetOnRepeatToggled(std::function<void()> handler) = 0;
};
