#include "Presenters/ControlBarPresenter.h"

ControlBarPresenter::ControlBarPresenter(IControlBarView& view, ControlBarModel& model, IVideoModel& video)
    : m_view(view), m_model(model), m_video(video) {}

ControlBarPresenter::~ControlBarPresenter()
{
    m_view.SetOnPlayPauseRequested({});
    m_view.SetOnStopRequested({});
    m_view.SetOnPrevRequested({});
    m_view.SetOnNextRequested({});
    m_view.SetOnSeekAbsolute({});
    m_view.SetOnVolumeChanged({});
    m_view.SetOnMuteToggled({});
    m_view.SetOnSpeedStep({});
    m_view.SetOnSpeedReset({});
    m_view.SetOnRepeatToggled({});
    m_view.SetOnSubtitleToggle({});
    m_view.SetOnPlaylistRequested({});
    m_view.SetOnSettingsRequested({});
}

void ControlBarPresenter::Initialize()
{
    m_view.SetOnPlayPauseRequested([this]() { PlayPause(); });
    m_view.SetOnStopRequested([this]() { Stop(); });
    m_view.SetOnPrevRequested([this]() { if (m_onPrevious) m_onPrevious(); });
    m_view.SetOnNextRequested([this]() { if (m_onNext) m_onNext(); });
    m_view.SetOnSeekAbsolute([this](double seconds) { SeekAbsolute(seconds); });
    m_view.SetOnVolumeChanged([this](int volume) { SetVolume(volume); });
    m_view.SetOnMuteToggled([this]() { ToggleMute(); });
    m_view.SetOnSpeedStep([this](double delta) { StepSpeed(delta); });
    m_view.SetOnSpeedReset([this]() { ResetSpeed(); });
    m_view.SetOnRepeatToggled([this]() { ToggleRepeat(); });
    m_view.SetOnSubtitleToggle([this]() { ToggleSubtitle(); });
    m_view.SetOnPlaylistRequested([this]() { if (m_onPlaylist) m_onPlaylist(); });
    m_view.SetOnSettingsRequested([this]() { if (m_onSettings) m_onSettings(); });
}

void ControlBarPresenter::Refresh(bool topMost, bool playlistOpen)
{
    ControlBarState state{};
    state.playbackState = m_video.GetState();
    state.position = m_video.GetPosition();
    state.duration = m_video.GetDuration();
    state.volume = m_video.GetVolume();
    state.muted = m_video.IsMuted();
    state.speed = m_video.GetSpeed();
    state.repeatEnabled = m_video.IsRepeatEnabled();
    state.abLoopA = m_video.GetABLoopA();
    state.abLoopB = m_video.GetABLoopB();
    state.subtitleVisible = m_video.IsSubtitleVisible();
    state.subtitleDelay = m_video.GetSubtitleDelay();
    state.topMost = topMost;
    state.playlistOpen = playlistOpen;
    if (m_model.UpdateState(state)) m_view.Render(m_model.GetState());
}

void ControlBarPresenter::Notify(bool saveSettings)
{
    if (m_onStateChanged) m_onStateChanged(saveSettings);
}

void ControlBarPresenter::PlayPause()
{
    if (!m_video.HasMedia() || m_video.GetState() == PlaybackState::Stopped)
    {
        if (m_onOpenFile) m_onOpenFile();
        return;
    }
    if (m_video.GetState() == PlaybackState::Playing) m_video.Pause();
    else m_video.Play();
    Notify();
}

void ControlBarPresenter::Stop() { m_video.Stop(); Notify(); }
void ControlBarPresenter::SeekAbsolute(double seconds) { m_video.SeekAbsolute(seconds); Notify(); }
void ControlBarPresenter::SeekRelative(double seconds) { m_video.SeekRelative(seconds); Notify(); }
void ControlBarPresenter::SetVolume(int volume) { m_video.SetMuted(false); m_video.SetVolume(volume); Notify(true); }
void ControlBarPresenter::StepVolume(int delta) { SetVolume(m_video.GetVolume() + delta); }
void ControlBarPresenter::ToggleMute() { m_video.SetMuted(!m_video.IsMuted()); Notify(true); }
void ControlBarPresenter::StepSpeed(double delta) { m_video.SetSpeed(m_video.GetSpeed() + delta); Notify(true); }
void ControlBarPresenter::ResetSpeed() { m_video.SetSpeed(1.0); Notify(true); }
void ControlBarPresenter::ToggleRepeat() { m_video.SetRepeat(!m_video.IsRepeatEnabled()); Notify(true); }
void ControlBarPresenter::ToggleSubtitle() { m_video.SetSubtitleVisible(!m_video.IsSubtitleVisible()); Notify(true); }
void ControlBarPresenter::StepSubtitleDelay(double delta) { m_video.AdjustSubtitleDelay(delta); Notify(); }
void ControlBarPresenter::StepSubtitleScale(double delta) { m_video.SetSubtitleScale(m_video.GetSubtitleScale() + delta); Notify(true); }
void ControlBarPresenter::StepSubtitlePosition(int delta) { m_video.AdjustSubtitlePos(delta); Notify(); }
void ControlBarPresenter::SetABLoop(int action)
{
    if (action == 1) m_video.SetABLoopA();
    else if (action == 2) m_video.SetABLoopB();
    else m_video.ClearABLoop();
    Notify();
}
