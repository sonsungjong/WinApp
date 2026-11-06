#include "Presenters/MainPresenter.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>

MainPresenter::MainPresenter(IMainView& view, IVideoModel& video, AppConfig& config, ConfigService& configService,
    ControlBarPresenter& controlBar, SettingsPresenter& settings, PlaylistPresenter& playlist)
    : m_view(view), m_model(video), m_config(config), m_configService(configService),
      m_controlBarPresenter(controlBar), m_settingsPresenter(settings), m_playlistPresenter(playlist) {}

MainPresenter::~MainPresenter()
{
    if (m_initialized) SaveConfig();
    m_playlistPresenter.SetOnChanged({});
    m_controlBarPresenter.SetOnStateChanged({});
    m_controlBarPresenter.SetOnOpenFileRequested({});
    m_controlBarPresenter.SetOnPrevRequested({});
    m_controlBarPresenter.SetOnNextRequested({});
    m_controlBarPresenter.SetOnPlaylistRequested({});
    m_controlBarPresenter.SetOnSettingsRequested({});
    m_view.SetOnOpenFileRequested({});
    m_view.SetOnOpenSubtitleRequested({});
    m_view.SetOnScreenshotRequested({});
    m_view.SetOnFilesDropped({});
    m_view.SetOnPlayPauseRequested({});
    m_view.SetOnStopRequested({});
    m_view.SetOnPrevRequested({});
    m_view.SetOnNextRequested({});
    m_view.SetOnSeekDirection({});
    m_view.SetOnVolumeStep({});
    m_view.SetOnRepeatToggled({});
    m_view.SetOnABLoopAction({});
    m_view.SetOnSubtitleToggle({});
    m_view.SetOnSubtitleDelayStep({});
    m_view.SetOnSubtitleScaleStep({});
    m_view.SetOnSubtitlePosStep({});
    m_view.SetOnTopMostToggled({});
    m_view.SetOnSettingsRequested({});
    m_view.SetOnPlaylistRequested({});
}

bool MainPresenter::Initialize()
{
    if (!m_model.Initialize(m_view.GetVideoHwnd()) ||
        !m_playlistPresenter.Initialize(m_view.GetInstance(), m_view.GetHwnd())) return false;
    m_controlBarPresenter.Initialize();
    ApplyConfig();

    m_view.SetOnOpenFileRequested([this]() { OnOpenFileRequested(); });
    m_view.SetOnOpenSubtitleRequested([this]() { OnOpenSubtitleRequested(); });
    m_view.SetOnScreenshotRequested([this]() { OnScreenshotRequested(); });
    m_view.SetOnFilesDropped([this](const auto& files) { OnFilesDropped(files); });
    m_view.SetOnPlayPauseRequested([this]() { m_controlBarPresenter.PlayPause(); });
    m_view.SetOnStopRequested([this]() { m_controlBarPresenter.Stop(); });
    m_view.SetOnPrevRequested([this]() { m_playlistPresenter.PlayPrevious(); });
    m_view.SetOnNextRequested([this]() { m_playlistPresenter.PlayNext(); });
    m_view.SetOnSeekDirection([this](int direction) { m_controlBarPresenter.SeekRelative(direction * m_config.seekStepSeconds); });
    m_view.SetOnVolumeStep([this](int delta) { m_controlBarPresenter.StepVolume(delta); });
    m_view.SetOnRepeatToggled([this]() { m_controlBarPresenter.ToggleRepeat(); });
    m_view.SetOnABLoopAction([this](int action) { m_controlBarPresenter.SetABLoop(action); });
    m_view.SetOnSubtitleToggle([this]() { m_controlBarPresenter.ToggleSubtitle(); });
    m_view.SetOnSubtitleDelayStep([this](double delta) { m_controlBarPresenter.StepSubtitleDelay(delta); });
    m_view.SetOnSubtitleScaleStep([this](double delta) { m_controlBarPresenter.StepSubtitleScale(delta); });
    m_view.SetOnSubtitlePosStep([this](int delta) { m_controlBarPresenter.StepSubtitlePosition(delta); });
    m_view.SetOnTopMostToggled([this]() { OnTopMostToggled(); });
    m_view.SetOnSettingsRequested([this]() { OnSettingsRequested(); });
    m_view.SetOnPlaylistRequested([this]() { m_playlistPresenter.Toggle(); });

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

void MainPresenter::Tick()
{
    if (!m_initialized || m_ticking) return;
    m_ticking = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{ m_ticking };
    m_model.ProcessEvents();
    m_view.UpdateFullscreenAutoHide();
    if (m_settingsDirty && GetTickCount64() - m_lastSettingsChange >= 300) SaveConfig();
    UpdateView();
}

void MainPresenter::ApplyConfig()
{
    m_model.SetRepeat(m_config.repeatEnabled);
    m_model.SetVolume(m_config.volume);
    m_model.SetMuted(m_config.muted);
    m_model.SetSpeed(m_config.speed);
    m_model.SetSubtitleVisible(m_config.subtitleVisible);
    m_model.SetSubtitleScale(m_config.subtitleScale);
    m_view.SetTopMost(m_config.topMost);
}

void MainPresenter::SyncConfig()
{
    m_config.repeatEnabled = m_model.IsRepeatEnabled();
    m_config.volume = m_model.GetVolume();
    m_config.muted = m_model.IsMuted();
    m_config.speed = m_model.GetSpeed();
    m_config.subtitleVisible = m_model.IsSubtitleVisible();
    m_config.subtitleScale = m_model.GetSubtitleScale();
}

void MainPresenter::SaveConfig()
{
    SyncConfig();
    m_configService.Save(m_config);
    m_settingsDirty = false;
}

void MainPresenter::OnPlaybackChanged(bool saveSettings)
{
    if (saveSettings)
    {
        SyncConfig();
        m_settingsDirty = true;
        m_lastSettingsChange = GetTickCount64();
    }
    UpdateView();
}

void MainPresenter::OnOpenFileRequested()
{
    m_playlistPresenter.OpenFiles(m_view.PromptOpenVideoFiles());
}

void MainPresenter::OnOpenSubtitleRequested()
{
    if (!m_model.HasMedia()) return;
    const auto path = m_view.PromptOpenSubtitleFile();
    if (!path.empty()) m_model.LoadSubtitleFile(path);
    UpdateView();
}

void MainPresenter::OnScreenshotRequested()
{
    if (m_model.GetState() == PlaybackState::Stopped) return;
    const auto path = m_view.PromptSaveScreenshotFile();
    if (!path.empty()) m_model.TakeScreenshot(path);
}

void MainPresenter::OnFilesDropped(const std::vector<std::wstring>& files)
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

void MainPresenter::OnTopMostToggled()
{
    m_config.topMost = !m_config.topMost;
    m_view.SetTopMost(m_config.topMost);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnSettingsRequested()
{
    SyncConfig();
    if (m_settingsPresenter.ShowSettingsDialog(m_view.GetInstance(), m_view.GetHwnd(), m_config))
    {
        ApplyConfig();
        m_settingsDirty = false;
        UpdateView();
    }
}

bool MainPresenter::IsSubtitleExtension(const std::wstring& path)
{
    auto extension = std::filesystem::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    return extension == L".smi" || extension == L".srt" || extension == L".ass" || extension == L".ssa" || extension == L".vtt" || extension == L".sub";
}

void MainPresenter::UpdateView()
{
    m_playlistPresenter.Refresh();
    m_view.UpdateMenuState(m_model.IsRepeatEnabled(), m_model.IsSubtitleVisible(), m_config.topMost);
    m_controlBarPresenter.Refresh(m_config.topMost, m_playlistPresenter.IsVisible());
    if (!m_model.HasMedia())
    {
        m_view.SetVideoVisible(false);
        m_view.SetWindowTitle(L"NextVideoPlayer");
        m_view.SetStatusText(L"동영상 파일을 열거나 창으로 드래그하세요.");
        return;
    }
    auto state = m_model.GetState();
    m_view.SetVideoVisible(state != PlaybackState::Stopped);
    const wchar_t* label = state == PlaybackState::Playing ? L"재생 중" : state == PlaybackState::Paused ? L"일시정지" : L"정지";
    auto name = std::filesystem::path(m_model.GetFilePath()).filename().wstring();
    auto text = name + L" (" + label + L")" + (m_model.IsRepeatEnabled() ? L" [반복 ON]" : L"");
    m_view.SetWindowTitle(L"NextVideoPlayer - " + text);
    m_view.SetStatusText(text);
}
