#pragma once
#include "Models/ConfigService.h"
#include "Models/IVideoModel.h"
#include "Presenters/ControlBarPresenter.h"
#include "Presenters/PlaylistPresenter.h"
#include "Presenters/SettingsPresenter.h"
#include "Views/IMainView.h"
#include "Views/IVideoPlayerView.h"

class VideoPlayerPresenter
{
public:
    VideoPlayerPresenter(IVideoPlayerView& player, IMainView& shell, IVideoModel& video, AppConfig& config,
        ConfigService& configService, ControlBarPresenter& controlBar, SettingsPresenter& settings,
        PlaylistPresenter& playlist);
    ~VideoPlayerPresenter();
    bool Initialize();
    void Tick();
    void HandleShellCommand(int commandId);
    void HandleShellKeyDown(WPARAM key);
    void HandleShellMouseWheel(int delta);
    void HandleShellDropFiles(HDROP hDrop);
    void OnShellFullscreenChanged(bool fullscreen);
    void OnShellActivationChanged(bool active);

private:
    void ApplyConfig();
    void SyncConfig();
    void SaveConfig();
    void OnPlaybackChanged(bool saveSettings);
    void OnOpenFileRequested();
    void OnOpenSubtitleRequested();
    void OnScreenshotRequested();
    void OnFilesDropped(const std::vector<std::wstring>& files);
    void OnTopMostToggled();
    void OnSettingsRequested();
    static bool IsSubtitleExtension(const std::wstring& path);
    void UpdateView();

    IVideoPlayerView& m_player;
    IMainView& m_shell;
    IVideoModel& m_model;
    AppConfig& m_config;
    ConfigService& m_configService;
    ControlBarPresenter& m_controlBarPresenter;
    SettingsPresenter& m_settingsPresenter;
    PlaylistPresenter& m_playlistPresenter;
    bool m_initialized = false;
    bool m_ticking = false;
    bool m_settingsDirty = false;
    ULONGLONG m_lastSettingsChange = 0;
};
