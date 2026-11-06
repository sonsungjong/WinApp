#pragma once

#include "framework.h"

class IVideoPlayerView
{
public:
    virtual ~IVideoPlayerView() = default;

    virtual HINSTANCE GetInstance() const = 0;
    virtual HWND GetHwnd() const = 0;
    virtual HWND GetVideoHwnd() const = 0;
    virtual void SetBounds(int x, int y, int width, int height) = 0;
    virtual void SetVideoVisible(bool visible) = 0;
    virtual void SetStatusText(const std::wstring& statusText) = 0;
    virtual void SetFullscreenActive(bool active) = 0;
    virtual bool IsFullscreenActive() const = 0;
    virtual void UpdateFullscreenAutoHide() = 0;

    virtual std::vector<std::wstring> PromptOpenVideoFiles() = 0;
    virtual std::wstring PromptOpenSubtitleFile() = 0;
    virtual std::wstring PromptSaveScreenshotFile() = 0;
    virtual void Render() = 0;

    virtual void HandleCommand(int commandId) = 0;
    virtual void HandleKeyDown(WPARAM key) = 0;
    virtual void HandleMouseWheel(int delta) = 0;
    virtual void HandleDropFiles(HDROP hDrop) = 0;

    virtual void SetOnPlaylistRequested(std::function<void()> handler) = 0;
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
    virtual void SetOnFullscreenToggleRequested(std::function<void()> handler) = 0;
};
