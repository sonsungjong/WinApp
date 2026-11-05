#pragma once

#include "framework.h"
#include "Models/AppConfig.h"
#include "Models/IVideoModel.h"

class IMainView
{
public:
    virtual ~IMainView() = default;

    virtual void Show(int cmdShow) = 0;
    virtual HINSTANCE GetInstance() const = 0;
    virtual HWND GetHwnd() const = 0;
    virtual HWND GetVideoHwnd() const = 0;
    virtual void SetVideoVisible(bool visible) = 0;
    virtual void SetWindowTitle(const std::wstring& title) = 0;
    virtual void SetStatusText(const std::wstring& statusText) = 0;
    virtual void UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost) = 0;
    virtual void UpdatePlaylistView(const std::vector<std::wstring>& items, int currentIndex) = 0;
    virtual bool IsPlaylistVisible() const = 0;
    virtual void SetTopMost(bool topMost) = 0;
    virtual void ToggleFullscreen() = 0;
    virtual void ExitFullscreen() = 0;
    virtual bool IsFullscreen() const = 0;
    virtual void UpdateFullscreenAutoHide() = 0;

    virtual std::wstring PromptOpenVideoFile() = 0;
    virtual std::wstring PromptOpenSubtitleFile() = 0;
    virtual std::wstring PromptSaveScreenshotFile() = 0;
    virtual void TogglePlaylistWindow() = 0;
    virtual void Render() = 0;

    virtual void SetOnOpenFileRequested(std::function<void()> handler) = 0;
    virtual void SetOnOpenSubtitleRequested(std::function<void()> handler) = 0;
    virtual void SetOnScreenshotRequested(std::function<void()> handler) = 0;
    virtual void SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) = 0;
    virtual void SetOnPlayPauseRequested(std::function<void()> handler) = 0;
    virtual void SetOnStopRequested(std::function<void()> handler) = 0;
    virtual void SetOnPrevRequested(std::function<void()> handler) = 0;
    virtual void SetOnNextRequested(std::function<void()> handler) = 0;
    virtual void SetOnSeekDirection(std::function<void(int)> handler) = 0;
    virtual void SetOnVolumeStep(std::function<void(int)> handler) = 0;
    virtual void SetOnRepeatToggled(std::function<void()> handler) = 0;
    virtual void SetOnABLoopAction(std::function<void(int)> handler) = 0;
    virtual void SetOnSubtitleToggle(std::function<void()> handler) = 0;
    virtual void SetOnSubtitleDelayStep(std::function<void(double)> handler) = 0;
    virtual void SetOnSubtitleScaleStep(std::function<void(double)> handler) = 0;
    virtual void SetOnSubtitlePosStep(std::function<void(int)> handler) = 0;
    virtual void SetOnTopMostToggled(std::function<void()> handler) = 0;
    virtual void SetOnSettingsRequested(std::function<void()> handler) = 0;
    virtual void SetOnPlaylistPlayIndex(std::function<void(int)> handler) = 0;
    virtual void SetOnPlaylistAddFiles(std::function<void()> handler) = 0;
    virtual void SetOnPlaylistRemoveIndex(std::function<void(int)> handler) = 0;
    virtual void SetOnPlaylistClear(std::function<void()> handler) = 0;
    virtual void SetOnPlaylistVisibilityChanged(std::function<void()> handler) = 0;
};
