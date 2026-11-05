#pragma once

#include "Models/ControlBarModel.h"
#include "Views/IControlBarView.h"

class ControlBarPresenter
{
public:
    ControlBarPresenter(IControlBarView& view, ControlBarModel& model);
    ~ControlBarPresenter() = default;

    bool Initialize();
    void UpdateState(const ControlBarState& state);
    const ControlBarState& GetState() const;

    void SetOnPlayPauseRequested(std::function<void()> handler);
    void SetOnStopRequested(std::function<void()> handler);
    void SetOnPrevRequested(std::function<void()> handler);
    void SetOnNextRequested(std::function<void()> handler);
    void SetOnSeekAbsolute(std::function<void(double)> handler);
    void SetOnVolumeChanged(std::function<void(int)> handler);
    void SetOnMuteToggled(std::function<void()> handler);
    void SetOnSpeedStep(std::function<void(double)> handler);
    void SetOnSpeedReset(std::function<void()> handler);
    void SetOnRepeatToggled(std::function<void()> handler);
    void SetOnSubtitleToggle(std::function<void()> handler);
    void SetOnPlaylistRequested(std::function<void()> handler);
    void SetOnSettingsRequested(std::function<void()> handler);

private:
    IControlBarView& m_view;
    ControlBarModel& m_model;

    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onPrevRequested;
    std::function<void()> m_onNextRequested;
    std::function<void(double)> m_onSeekAbsolute;
    std::function<void(int)> m_onVolumeChanged;
    std::function<void()> m_onMuteToggled;
    std::function<void(double)> m_onSpeedStep;
    std::function<void()> m_onSpeedReset;
    std::function<void()> m_onRepeatToggled;
    std::function<void()> m_onSubtitleToggle;
    std::function<void()> m_onPlaylistRequested;
    std::function<void()> m_onSettingsRequested;
};

