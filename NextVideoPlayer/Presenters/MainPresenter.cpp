#include "Presenters/MainPresenter.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>

MainPresenter::MainPresenter(
    IMainView& view,
    IVideoModel& model,
    ConfigService& configService,
    ControlBarPresenter& controlBarPresenter,
    SettingsPresenter& settingsPresenter)
    : m_view(view)
    , m_model(model)
    , m_configService(configService)
    , m_controlBarPresenter(controlBarPresenter)
    , m_settingsPresenter(settingsPresenter)
{
}

MainPresenter::~MainPresenter()
{
    SaveConfig();
}

bool MainPresenter::Initialize()
{
    m_config = m_configService.Load();

    if (!m_model.Initialize(m_view.GetVideoHwnd()))
    {
        return false;
    }

    if (!m_controlBarPresenter.Initialize())
    {
        return false;
    }

    ApplyConfig();

    // Bind MainView events
    m_view.SetOnOpenFileRequested([this]() { OnOpenFileRequested(); });
    m_view.SetOnOpenSubtitleRequested([this]() { OnOpenSubtitleRequested(); });
    m_view.SetOnScreenshotRequested([this]() { OnScreenshotRequested(); });
    m_view.SetOnFilesDropped([this](const std::vector<std::wstring>& files) { OnFilesDropped(files); });
    m_view.SetOnPlayPauseRequested([this]() { OnPlayPauseRequested(); });
    m_view.SetOnStopRequested([this]() { OnStopRequested(); });
    m_view.SetOnPrevRequested([this]() { OnPrevRequested(); });
    m_view.SetOnNextRequested([this]() { OnNextRequested(); });
    m_view.SetOnSeekDirection([this](int dir) { OnSeekDirection(dir); });
    m_view.SetOnVolumeStep([this](int delta) { OnVolumeStep(delta); });
    m_view.SetOnRepeatToggled([this]() { OnRepeatToggled(); });
    m_view.SetOnABLoopAction([this](int action) { OnABLoopAction(action); });
    m_view.SetOnSubtitleToggle([this]() { OnSubtitleToggle(); });
    m_view.SetOnSubtitleDelayStep([this](double delta) { OnSubtitleDelayStep(delta); });
    m_view.SetOnSubtitleScaleStep([this](double delta) { OnSubtitleScaleStep(delta); });
    m_view.SetOnSubtitlePosStep([this](int delta) { OnSubtitlePosStep(delta); });
    m_view.SetOnTopMostToggled([this]() { OnTopMostToggled(); });
    m_view.SetOnSettingsRequested([this]() { OnSettingsRequested(); });
    m_view.SetOnPlaylistPlayIndex([this](int idx) { OnPlaylistPlayIndex(idx); });
    m_view.SetOnPlaylistAddFiles([this]() { OnPlaylistAddFiles(); });
    m_view.SetOnPlaylistRemoveIndex([this](int idx) { OnPlaylistRemoveIndex(idx); });
    m_view.SetOnPlaylistClear([this]() { OnPlaylistClear(); });
    m_view.SetOnPlaylistVisibilityChanged([this]() { UpdateView(); });

    // Bind ControlBarPresenter events
    m_controlBarPresenter.SetOnPlayPauseRequested([this]() { OnPlayPauseRequested(); });
    m_controlBarPresenter.SetOnStopRequested([this]() { OnStopRequested(); });
    m_controlBarPresenter.SetOnPrevRequested([this]() { OnPrevRequested(); });
    m_controlBarPresenter.SetOnNextRequested([this]() { OnNextRequested(); });
    m_controlBarPresenter.SetOnSeekAbsolute([this](double sec) { OnSeekAbsolute(sec); });
    m_controlBarPresenter.SetOnVolumeChanged([this](int vol) { OnVolumeChanged(vol); });
    m_controlBarPresenter.SetOnMuteToggled([this]() { OnMuteToggled(); });
    m_controlBarPresenter.SetOnSpeedStep([this](double delta) { OnSpeedStep(delta); });
    m_controlBarPresenter.SetOnSpeedReset([this]() { OnSpeedReset(); });
    m_controlBarPresenter.SetOnRepeatToggled([this]() { OnRepeatToggled(); });
    m_controlBarPresenter.SetOnSubtitleToggle([this]() { OnSubtitleToggle(); });
    m_controlBarPresenter.SetOnPlaylistRequested([this]() { m_view.TogglePlaylistWindow(); });
    m_controlBarPresenter.SetOnSettingsRequested([this]() { OnSettingsRequested(); });

    UpdateView();
    return true;
}

void MainPresenter::Tick()
{
    m_model.ProcessEvents();
    m_view.UpdateFullscreenAutoHide();
    UpdateView();
}

bool MainPresenter::IsActiveLoop() const
{
    return m_model.GetState() == PlaybackState::Playing || m_view.IsFullscreen();
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

void MainPresenter::SaveConfig()
{
    m_config.repeatEnabled = m_model.IsRepeatEnabled();
    m_config.volume = m_model.GetVolume();
    m_config.muted = m_model.IsMuted();
    m_config.speed = m_model.GetSpeed();
    m_config.subtitleVisible = m_model.IsSubtitleVisible();
    m_config.subtitleScale = m_model.GetSubtitleScale();
    m_configService.Save(m_config);
}

void MainPresenter::OnOpenFileRequested()
{
    std::wstring selectedPath = m_view.PromptOpenVideoFile();
    if (!selectedPath.empty())
    {
        if (m_model.OpenFile(selectedPath))
        {
            m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
            UpdateView();
        }
    }
}

void MainPresenter::OnOpenSubtitleRequested()
{
    if (!m_model.HasMedia())
    {
        return;
    }

    std::wstring subPath = m_view.PromptOpenSubtitleFile();
    if (!subPath.empty())
    {
        m_model.LoadSubtitleFile(subPath);
        UpdateView();
    }
}

void MainPresenter::OnScreenshotRequested()
{
    if (m_model.GetState() == PlaybackState::Stopped)
    {
        return;
    }

    std::wstring savePath = m_view.PromptSaveScreenshotFile();
    if (!savePath.empty())
    {
        m_model.TakeScreenshot(savePath);
    }
}

void MainPresenter::OnFilesDropped(const std::vector<std::wstring>& files)
{
    std::wstring firstVideo;

    for (const auto& file : files)
    {
        if (IsSubtitleExtension(file))
        {
            m_model.LoadSubtitleFile(file);
        }
        else
        {
            if (firstVideo.empty())
            {
                firstVideo = file;
            }
            m_model.AddToPlaylist(file);
        }
    }

    if (!firstVideo.empty())
    {
        m_model.OpenFile(firstVideo);
        m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
    }

    UpdateView();
}

void MainPresenter::OnPlayPauseRequested()
{
    if (!m_model.HasMedia())
    {
        OnOpenFileRequested();
        return;
    }

    if (m_model.GetState() == PlaybackState::Playing)
    {
        m_model.Pause();
    }
    else
    {
        m_model.Play();
    }

    UpdateView();
}

void MainPresenter::OnStopRequested()
{
    if (!m_model.HasMedia())
    {
        return;
    }

    m_model.Stop();
    UpdateView();
}

void MainPresenter::OnPrevRequested()
{
    if (m_model.PlayPrevious())
    {
        m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
        UpdateView();
    }
}

void MainPresenter::OnNextRequested()
{
    if (m_model.PlayNext())
    {
        m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
        UpdateView();
    }
}

void MainPresenter::OnSeekAbsolute(double seconds)
{
    m_model.SeekAbsolute(seconds);
    UpdateView();
}

void MainPresenter::OnSeekDirection(int direction)
{
    m_model.SeekRelative(static_cast<double>(direction * m_config.seekStepSeconds));
    UpdateView();
}

void MainPresenter::OnVolumeChanged(int volume)
{
    m_model.SetMuted(false);
    m_model.SetVolume(volume);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnVolumeStep(int delta)
{
    m_model.SetMuted(false);
    m_model.SetVolume(m_model.GetVolume() + delta);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnMuteToggled()
{
    m_model.SetMuted(!m_model.IsMuted());
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnSpeedStep(double delta)
{
    m_model.SetSpeed(m_model.GetSpeed() + delta);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnSpeedReset()
{
    m_model.SetSpeed(1.0);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnRepeatToggled()
{
    m_model.SetRepeat(!m_model.IsRepeatEnabled());
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnABLoopAction(int action)
{
    if (action == 1)
    {
        m_model.SetABLoopA();
    }
    else if (action == 2)
    {
        m_model.SetABLoopB();
    }
    else
    {
        m_model.ClearABLoop();
    }
    UpdateView();
}

void MainPresenter::OnSubtitleToggle()
{
    m_model.SetSubtitleVisible(!m_model.IsSubtitleVisible());
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnSubtitleDelayStep(double delta)
{
    m_model.AdjustSubtitleDelay(delta);
    UpdateView();
}

void MainPresenter::OnSubtitleScaleStep(double delta)
{
    m_model.SetSubtitleScale(m_model.GetSubtitleScale() + delta);
    SaveConfig();
    UpdateView();
}

void MainPresenter::OnSubtitlePosStep(int delta)
{
    m_model.AdjustSubtitlePos(delta);
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
    m_config.repeatEnabled = m_model.IsRepeatEnabled();
    m_config.subtitleVisible = m_model.IsSubtitleVisible();

    if (m_settingsPresenter.ShowSettingsDialog(m_view.GetInstance(), m_view.GetHwnd(), m_config))
    {
        ApplyConfig();
        SaveConfig();
        UpdateView();
    }
}

void MainPresenter::OnPlaylistPlayIndex(int index)
{
    if (m_model.PlayPlaylistIndex(index))
    {
        m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
        UpdateView();
    }
}

void MainPresenter::OnPlaylistAddFiles()
{
    std::wstring path = m_view.PromptOpenVideoFile();
    if (!path.empty())
    {
        m_model.AddToPlaylist(path);
        m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
    }
}

void MainPresenter::OnPlaylistRemoveIndex(int index)
{
    m_model.RemoveFromPlaylist(index);
    m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
}

void MainPresenter::OnPlaylistClear()
{
    m_model.ClearPlaylist();
    m_view.UpdatePlaylistView(m_model.GetPlaylist(), m_model.GetCurrentPlaylistIndex());
}

bool MainPresenter::IsSubtitleExtension(const std::wstring& filePath)
{
    std::wstring ext = std::filesystem::path(filePath).extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext == L".smi" || ext == L".srt" || ext == L".ass" || ext == L".ssa" || ext == L".vtt" || ext == L".sub";
}

void MainPresenter::UpdateView()
{
    ControlBarState barState{};
    barState.playbackState = m_model.GetState();
    barState.position = m_model.GetPosition();
    barState.duration = m_model.GetDuration();
    barState.volume = m_model.GetVolume();
    barState.muted = m_model.IsMuted();
    barState.speed = m_model.GetSpeed();
    barState.repeatEnabled = m_model.IsRepeatEnabled();
    barState.abLoopA = m_model.GetABLoopA();
    barState.abLoopB = m_model.GetABLoopB();
    barState.subtitleVisible = m_model.IsSubtitleVisible();
    barState.subtitleDelay = m_model.GetSubtitleDelay();
    barState.topMost = m_config.topMost;
    barState.playlistOpen = m_view.IsPlaylistVisible();

    m_view.UpdateMenuState(barState.repeatEnabled, barState.subtitleVisible, barState.topMost);
    m_controlBarPresenter.UpdateState(barState);

    if (!m_model.HasMedia())
    {
        m_view.SetVideoVisible(false);
        m_view.SetWindowTitle(L"NextVideoPlayer");
        m_view.SetStatusText(L"동영상 파일을 열거나 창으로 드래그하세요.");
        return;
    }

    std::filesystem::path path(m_model.GetFilePath());
    std::wstring fileName = path.filename().wstring();
    PlaybackState state = m_model.GetState();

    m_view.SetVideoVisible(state != PlaybackState::Stopped);

    std::wstring stateLabel;
    switch (state)
    {
    case PlaybackState::Playing:
        stateLabel = L"재생 중";
        break;
    case PlaybackState::Paused:
        stateLabel = L"일시정지";
        break;
    case PlaybackState::Stopped:
        stateLabel = L"정지";
        break;
    }

    std::wstring repeatLabel = m_model.IsRepeatEnabled() ? L" [반복 ON]" : L"";
    m_view.SetWindowTitle(L"NextVideoPlayer - " + fileName + L" (" + stateLabel + L")" + repeatLabel);
    m_view.SetStatusText(fileName + L" (" + stateLabel + L")" + repeatLabel);
}
