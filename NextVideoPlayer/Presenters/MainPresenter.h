#pragma once

#include "Models/ConfigService.h"
#include "Models/IVideoModel.h"
#include "Presenters/ControlBarPresenter.h"
#include "Presenters/SettingsPresenter.h"
#include "Views/IMainView.h"

class MainPresenter
{
public:
    MainPresenter(
        IMainView& view,
        IVideoModel& model,
        ConfigService& configService,
        ControlBarPresenter& controlBarPresenter,
        SettingsPresenter& settingsPresenter);
    ~MainPresenter();

    bool Initialize();
    void Tick();
    bool IsActiveLoop() const;

private:
    void ApplyConfig();
    void SaveConfig();

    void OnOpenFileRequested();
    void OnOpenSubtitleRequested();
    void OnScreenshotRequested();
    void OnFilesDropped(const std::vector<std::wstring>& files);
    void OnPlayPauseRequested();
    void OnStopRequested();
    void OnPrevRequested();
    void OnNextRequested();
    void OnSeekAbsolute(double seconds);
    void OnSeekDirection(int direction);
    void OnVolumeChanged(int volume);
    void OnVolumeStep(int delta);
    void OnMuteToggled();
    void OnSpeedStep(double delta);
    void OnSpeedReset();
    void OnRepeatToggled();
    void OnABLoopAction(int action);
    void OnSubtitleToggle();
    void OnSubtitleDelayStep(double delta);
    void OnSubtitleScaleStep(double delta);
    void OnSubtitlePosStep(int delta);
    void OnTopMostToggled();
    void OnSettingsRequested();

    void OnPlaylistPlayIndex(int index);
    void OnPlaylistAddFiles();
    void OnPlaylistRemoveIndex(int index);
    void OnPlaylistClear();

    static bool IsSubtitleExtension(const std::wstring& filePath);
    void UpdateView();

    IMainView& m_view;
    IVideoModel& m_model;
    ConfigService& m_configService;
    ControlBarPresenter& m_controlBarPresenter;
    SettingsPresenter& m_settingsPresenter;
    AppConfig m_config{};
};
