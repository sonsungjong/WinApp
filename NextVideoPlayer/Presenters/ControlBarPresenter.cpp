#include "Presenters/ControlBarPresenter.h"

ControlBarPresenter::ControlBarPresenter(IControlBarView& view, ControlBarModel& model)
    : m_view(view)
    , m_model(model)
{
}

bool ControlBarPresenter::Initialize()
{
    m_view.SetOnPlayPauseRequested([this]() { if (m_onPlayPauseRequested) m_onPlayPauseRequested(); });
    m_view.SetOnStopRequested([this]() { if (m_onStopRequested) m_onStopRequested(); });
    m_view.SetOnPrevRequested([this]() { if (m_onPrevRequested) m_onPrevRequested(); });
    m_view.SetOnNextRequested([this]() { if (m_onNextRequested) m_onNextRequested(); });
    m_view.SetOnSeekAbsolute([this](double sec) { if (m_onSeekAbsolute) m_onSeekAbsolute(sec); });
    m_view.SetOnVolumeChanged([this](int vol) { if (m_onVolumeChanged) m_onVolumeChanged(vol); });
    m_view.SetOnMuteToggled([this]() { if (m_onMuteToggled) m_onMuteToggled(); });
    m_view.SetOnSpeedStep([this](double delta) { if (m_onSpeedStep) m_onSpeedStep(delta); });
    m_view.SetOnSpeedReset([this]() { if (m_onSpeedReset) m_onSpeedReset(); });
    m_view.SetOnRepeatToggled([this]() { if (m_onRepeatToggled) m_onRepeatToggled(); });
    m_view.SetOnSubtitleToggle([this]() { if (m_onSubtitleToggle) m_onSubtitleToggle(); });
    m_view.SetOnPlaylistRequested([this]() { if (m_onPlaylistRequested) m_onPlaylistRequested(); });
    m_view.SetOnSettingsRequested([this]() { if (m_onSettingsRequested) m_onSettingsRequested(); });
    return true;
}

void ControlBarPresenter::UpdateState(const ControlBarState& state)
{
    if (m_model.UpdateState(state))
    {
        m_view.Render(m_model.GetState());
    }
}

const ControlBarState& ControlBarPresenter::GetState() const
{
    return m_model.GetState();
}

void ControlBarPresenter::SetOnPlayPauseRequested(std::function<void()> handler) { m_onPlayPauseRequested = std::move(handler); }
void ControlBarPresenter::SetOnStopRequested(std::function<void()> handler) { m_onStopRequested = std::move(handler); }
void ControlBarPresenter::SetOnPrevRequested(std::function<void()> handler) { m_onPrevRequested = std::move(handler); }
void ControlBarPresenter::SetOnNextRequested(std::function<void()> handler) { m_onNextRequested = std::move(handler); }
void ControlBarPresenter::SetOnSeekAbsolute(std::function<void(double)> handler) { m_onSeekAbsolute = std::move(handler); }
void ControlBarPresenter::SetOnVolumeChanged(std::function<void(int)> handler) { m_onVolumeChanged = std::move(handler); }
void ControlBarPresenter::SetOnMuteToggled(std::function<void()> handler) { m_onMuteToggled = std::move(handler); }
void ControlBarPresenter::SetOnSpeedStep(std::function<void(double)> handler) { m_onSpeedStep = std::move(handler); }
void ControlBarPresenter::SetOnSpeedReset(std::function<void()> handler) { m_onSpeedReset = std::move(handler); }
void ControlBarPresenter::SetOnRepeatToggled(std::function<void()> handler) { m_onRepeatToggled = std::move(handler); }
void ControlBarPresenter::SetOnSubtitleToggle(std::function<void()> handler) { m_onSubtitleToggle = std::move(handler); }
void ControlBarPresenter::SetOnPlaylistRequested(std::function<void()> handler) { m_onPlaylistRequested = std::move(handler); }
void ControlBarPresenter::SetOnSettingsRequested(std::function<void()> handler) { m_onSettingsRequested = std::move(handler); }

