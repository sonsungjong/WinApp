#pragma once
#include "Models/ControlBarModel.h"
#include "Views/IControlBarView.h"

class ControlBarPresenter
{
public:
    ControlBarPresenter(IControlBarView& view, ControlBarModel& model, IVideoModel& video);
    ~ControlBarPresenter();
    void Initialize();
    void Refresh(bool topMost, bool playlistOpen);
    void PlayPause();
    void Stop();
    void SeekAbsolute(double seconds);
    void SeekRelative(double seconds);
    void SetVolume(int volume);
    void StepVolume(int delta);
    void ToggleMute();
    void StepSpeed(double delta);
    void ResetSpeed();
    void ToggleRepeat();
    void SetABLoop(int action);
    void ToggleSubtitle();
    void StepSubtitleDelay(double delta);
    void StepSubtitleScale(double delta);
    void StepSubtitlePosition(int delta);
    void SetOnOpenFileRequested(std::function<void()> handler) { m_onOpenFile = std::move(handler); }
    void SetOnPrevRequested(std::function<void()> handler) { m_onPrevious = std::move(handler); }
    void SetOnNextRequested(std::function<void()> handler) { m_onNext = std::move(handler); }
    void SetOnPlaylistRequested(std::function<void()> handler) { m_onPlaylist = std::move(handler); }
    void SetOnSettingsRequested(std::function<void()> handler) { m_onSettings = std::move(handler); }
    void SetOnStateChanged(std::function<void(bool)> handler) { m_onStateChanged = std::move(handler); }

private:
    void Notify(bool saveSettings = false);
    IControlBarView& m_view;
    ControlBarModel& m_model;
    IVideoModel& m_video;
    std::function<void()> m_onOpenFile, m_onPrevious, m_onNext, m_onPlaylist, m_onSettings;
    std::function<void(bool)> m_onStateChanged;
};
