#include "Presenters/VideoPlayerPresenter.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>

VideoPlayerPresenter::VideoPlayerPresenter(IVideoPlayerView& player, IMainView& shell, IVideoModel& video, AppConfig& config,
    ConfigService& configService, ControlBarPresenter& controlBar, SettingsPresenter& settings, PlaylistPresenter& playlist)
    : m_player(player), m_shell(shell), m_model(video), m_config(config), m_configService(configService),
      m_controlBarPresenter(controlBar), m_settingsPresenter(settings), m_playlistPresenter(playlist) {}

VideoPlayerPresenter::~VideoPlayerPresenter()
{
    if (m_initialized) SaveConfig();
    m_playlistPresenter.SetOnChanged({});
    m_controlBarPresenter.SetOnStateChanged({});
    m_controlBarPresenter.SetOnOpenFileRequested({});
    m_controlBarPresenter.SetOnPrevRequested({});
    m_controlBarPresenter.SetOnNextRequested({});
    m_controlBarPresenter.SetOnPlaylistRequested({});
    m_controlBarPresenter.SetOnSettingsRequested({});
    m_player.SetOnOpenFileRequested({});
    m_player.SetOnOpenSubtitleRequested({});
    m_player.SetOnScreenshotRequested({});
    m_player.SetOnFilesDropped({});
    m_player.SetOnPlayPauseRequested({});
    m_player.SetOnStopRequested({});
    m_player.SetOnPrevRequested({});
    m_player.SetOnNextRequested({});
    m_player.SetOnSeekDirection({});
    m_player.SetOnVolumeStep({});
    m_player.SetOnRepeatToggled({});
    m_player.SetOnABLoopAction({});
    m_player.SetOnSubtitleToggle({});
    m_player.SetOnSubtitleDelayStep({});
    m_player.SetOnSubtitleScaleStep({});
    m_player.SetOnSubtitlePosStep({});
    m_player.SetOnTopMostToggled({});
    m_player.SetOnSettingsRequested({});
    m_player.SetOnPlaylistRequested({});
    m_player.SetOnFullscreenToggleRequested({});
}

bool VideoPlayerPresenter::Initialize()
{
    if (!m_model.Initialize(m_player.GetVideoHwnd()) ||
        !m_playlistPresenter.Initialize(m_player.GetInstance(), m_shell.GetHwnd())) return false;
    m_controlBarPresenter.Initialize();
    ApplyConfig();

    m_player.SetOnOpenFileRequested([this]() { OnOpenFileRequested(); });
    m_player.SetOnOpenSubtitleRequested([this]() { OnOpenSubtitleRequested(); });
    m_player.SetOnScreenshotRequested([this]() { OnScreenshotRequested(); });
    m_player.SetOnFilesDropped([this](const auto& files) { OnFilesDropped(files); });
    m_player.SetOnPlayPauseRequested([this]() { m_controlBarPresenter.PlayPause(); });
    m_player.SetOnStopRequested([this]() { m_controlBarPresenter.Stop(); });
    m_player.SetOnPrevRequested([this]() { m_playlistPresenter.PlayPrevious(); });
    m_player.SetOnNextRequested([this]() { m_playlistPresenter.PlayNext(); });
    m_player.SetOnSeekDirection([this](int direction) { m_controlBarPresenter.SeekRelative(direction * m_config.seekStepSeconds); });
    m_player.SetOnVolumeStep([this](int delta) { m_controlBarPresenter.StepVolume(delta); });
    m_player.SetOnRepeatToggled([this]() { m_controlBarPresenter.ToggleRepeat(); });
    m_player.SetOnABLoopAction([this](int action) { m_controlBarPresenter.SetABLoop(action); });
    m_player.SetOnSubtitleToggle([this]() { m_controlBarPresenter.ToggleSubtitle(); });
    m_player.SetOnSubtitleDelayStep([this](double delta) { m_controlBarPresenter.StepSubtitleDelay(delta); });
    m_player.SetOnSubtitleScaleStep([this](double delta) { m_controlBarPresenter.StepSubtitleScale(delta); });
    m_player.SetOnSubtitlePosStep([this](int delta) { m_controlBarPresenter.StepSubtitlePosition(delta); });
    m_player.SetOnTopMostToggled([this]() { OnTopMostToggled(); });
    m_player.SetOnSettingsRequested([this]() { OnSettingsRequested(); });
    m_player.SetOnPlaylistRequested([this]() { m_playlistPresenter.Toggle(); });
    m_player.SetOnFullscreenToggleRequested([this]() { m_shell.ToggleFullscreen(); });

    m_controlBarPresenter.SetOnOpenFileRequested([this]() {
        if (!m_playlistPresenter.PlayCurrentOrFirst()) OnOpenFileRequested();
    });
    m_controlBarPresenter.SetOnPrevRequested([this]() { m_playlistPresenter.PlayPrevious(); });
    m_controlBarPresenter.SetOnNextRequested([this]() { m_playlistPresenter.PlayNext(); });
    m_controlBarPresenter.SetOnPlaylistRequested([this]() { m_playlistPresenter.Toggle(); });
    m_controlBarPresenter.SetOnSettingsRequested([this]() { OnSettingsRequested(); });
    m_controlBarPresenter.SetOnStateChanged([this](bool save) { OnPlaybackChanged(save); });
    m_playlistPresenter.SetOnChanged([this]() { UpdateView(); });
    m_initialized = true;
    UpdateView();
    return true;
}

void VideoPlayerPresenter::Tick()
{
    if (!m_initialized || m_ticking) return;
    m_ticking = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{ m_ticking };
    m_model.ProcessEvents();
    m_player.UpdateFullscreenAutoHide();
    if (m_settingsDirty && GetTickCount64() - m_lastSettingsChange >= 300) SaveConfig();
    UpdateView();
}

void VideoPlayerPresenter::HandleShellCommand(int commandId)
{
    m_player.HandleCommand(commandId);
}

void VideoPlayerPresenter::HandleShellKeyDown(WPARAM key)
{
    m_player.HandleKeyDown(key);
}

void VideoPlayerPresenter::HandleShellMouseWheel(int delta)
{
    m_player.HandleMouseWheel(delta);
}

void VideoPlayerPresenter::HandleShellDropFiles(HDROP hDrop)
{
    m_player.HandleDropFiles(hDrop);
}

void VideoPlayerPresenter::OnShellFullscreenChanged(bool fullscreen)
{
    m_player.SetFullscreenActive(fullscreen);
}

void VideoPlayerPresenter::ApplyConfig()
{
    m_model.SetRepeat(m_config.repeatEnabled);
    m_model.SetVolume(m_config.volume);
    m_model.SetMuted(m_config.muted);
    m_model.SetSpeed(m_config.speed);
    m_model.SetSubtitleVisible(m_config.subtitleVisible);
    m_model.SetSubtitleScale(m_config.subtitleScale);
    m_shell.SetTopMost(m_config.topMost);
}

void VideoPlayerPresenter::SyncConfig()
{
    m_config.repeatEnabled = m_model.IsRepeatEnabled();
    m_config.volume = m_model.GetVolume();
    m_config.muted = m_model.IsMuted();
    m_config.speed = m_model.GetSpeed();
    m_config.subtitleVisible = m_model.IsSubtitleVisible();
    m_config.subtitleScale = m_model.GetSubtitleScale();
}

void VideoPlayerPresenter::SaveConfig()
{
    SyncConfig();
    m_configService.Save(m_config);
    m_settingsDirty = false;
}

void VideoPlayerPresenter::OnPlaybackChanged(bool saveSettings)
{
    if (saveSettings)
    {
        SyncConfig();
        m_settingsDirty = true;
        m_lastSettingsChange = GetTickCount64();
    }
    UpdateView();
}

void VideoPlayerPresenter::OnOpenFileRequested()
{
    m_playlistPresenter.OpenFiles(m_player.PromptOpenVideoFiles());
}

void VideoPlayerPresenter::OnOpenSubtitleRequested()
{
    if (!m_model.HasMedia()) return;
    const auto path = m_player.PromptOpenSubtitleFile();
    if (!path.empty()) m_model.LoadSubtitleFile(path);
    UpdateView();
}

void VideoPlayerPresenter::OnScreenshotRequested()
{
    if (m_model.GetState() == PlaybackState::Stopped) return;
    const auto path = m_player.PromptSaveScreenshotFile();
    if (!path.empty()) m_model.TakeScreenshot(path);
}

void VideoPlayerPresenter::OnFilesDropped(const std::vector<std::wstring>& files)
{
    std::vector<std::wstring> videos;
    for (const auto& file : files)
    {
        if (!IsSubtitleExtension(file))
        {
            videos.push_back(file);
        }
    }
    if (!videos.empty()) m_playlistPresenter.OpenFiles(videos);
    for (const auto& file : files)
        if (IsSubtitleExtension(file)) m_model.LoadSubtitleFile(file);
    UpdateView();
}

void VideoPlayerPresenter::OnTopMostToggled()
{
    m_config.topMost = !m_config.topMost;
    m_shell.SetTopMost(m_config.topMost);
    SaveConfig();
    UpdateView();
}

void VideoPlayerPresenter::OnSettingsRequested()
{
    SyncConfig();
    if (m_settingsPresenter.ShowSettingsDialog(m_player.GetInstance(), m_shell.GetHwnd(), m_config))
    {
        ApplyConfig();
        m_settingsDirty = false;
        UpdateView();
    }
}

bool VideoPlayerPresenter::IsSubtitleExtension(const std::wstring& path)
{
    auto extension = std::filesystem::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    return extension == L".smi" || extension == L".srt" || extension == L".ass" || extension == L".ssa" || extension == L".vtt" || extension == L".sub";
}

void VideoPlayerPresenter::UpdateView()
{
    m_playlistPresenter.Refresh();
    m_shell.UpdateMenuState(m_model.IsRepeatEnabled(), m_model.IsSubtitleVisible(), m_config.topMost);
    m_controlBarPresenter.Refresh(m_config.topMost, m_playlistPresenter.IsVisible());
    if (!m_model.HasMedia())
    {
        m_player.SetVideoVisible(false);
        m_shell.SetWindowTitle(L"NextVideoPlayer");
        m_player.SetStatusText(L"동영상 파일을 열거나 창으로 드래그하세요.");
        return;
    }
    auto state = m_model.GetState();
    m_player.SetVideoVisible(state != PlaybackState::Stopped);
    const wchar_t* label = state == PlaybackState::Playing ? L"재생 중" : state == PlaybackState::Paused ? L"일시정지" : L"정지";
    auto name = std::filesystem::path(m_model.GetFilePath()).filename().wstring();
    auto text = name + L" (" + label + L")" + (m_model.IsRepeatEnabled() ? L" [반복 ON]" : L"");
    m_shell.SetWindowTitle(L"NextVideoPlayer - " + text);
    m_player.SetStatusText(text);
}
