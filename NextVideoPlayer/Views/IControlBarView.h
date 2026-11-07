#pragma once

#include "framework.h"
#include "Models/ControlBarModel.h"

class IControlBarView
{
public:
    static constexpr int kDefaultHeight = 76;

    virtual ~IControlBarView() = default;

    virtual bool Initialize(HINSTANCE hInstance, HWND parentHwnd, int x, int y, int width, int height) = 0;
    virtual HWND GetHwnd() const = 0;
    virtual void SetBounds(int x, int y, int width, int height, bool visible) = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual bool IsVisible() const = 0;
    virtual bool IsDragging() const = 0;
    virtual void Render(const ControlBarState& state) = 0;
    virtual void Invalidate() = 0;

    virtual void SetOnPlayPauseRequested(std::function<void()> handler) = 0;
    virtual void SetOnStopRequested(std::function<void()> handler) = 0;
    virtual void SetOnPrevRequested(std::function<void()> handler) = 0;
    virtual void SetOnNextRequested(std::function<void()> handler) = 0;
    virtual void SetOnSeekAbsolute(std::function<void(double)> handler) = 0;
    virtual void SetOnVolumeChanged(std::function<void(int)> handler) = 0;
    virtual void SetOnMuteToggled(std::function<void()> handler) = 0;
    virtual void SetOnSpeedStep(std::function<void(int)> handler) = 0;
    virtual void SetOnSpeedReset(std::function<void()> handler) = 0;
    virtual void SetOnRepeatToggled(std::function<void()> handler) = 0;
    virtual void SetOnSubtitleToggle(std::function<void()> handler) = 0;
    virtual void SetOnPlaylistRequested(std::function<void()> handler) = 0;
    virtual void SetOnSettingsRequested(std::function<void()> handler) = 0;
};

