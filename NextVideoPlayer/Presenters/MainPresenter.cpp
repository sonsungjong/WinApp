#include "Presenters/MainPresenter.h"
#include <filesystem>

MainPresenter::MainPresenter(IMainView& view, IVideoModel& model)
    : m_view(view)
    , m_model(model)
{
}

void MainPresenter::Initialize()
{
    m_view.SetOnOpenFileRequested([this]() { OnOpenFileRequested(); });
    m_view.SetOnFileDropped([this](const std::wstring& path) { OnFileDropped(path); });
    m_view.SetOnPlayPauseRequested([this]() { OnPlayPauseRequested(); });
    m_view.SetOnStopRequested([this]() { OnStopRequested(); });
    m_view.SetOnRepeatToggled([this]() { OnRepeatToggled(); });

    m_view.SetRepeatChecked(m_model.IsRepeatEnabled());
    UpdateView();
}

void MainPresenter::OnPlaybackEnded()
{
    if (m_model.IsRepeatEnabled() && m_model.HasMedia())
    {
        m_model.Stop();
        m_model.Play();
    }
    else
    {
        m_model.Stop();
    }

    UpdateView();
}

void MainPresenter::OnOpenFileRequested()
{
    std::wstring selectedPath = m_view.PromptOpenVideoFile();
    if (!selectedPath.empty())
    {
        LoadAndPlayFile(selectedPath);
    }
}

void MainPresenter::OnFileDropped(const std::wstring& filePath)
{
    LoadAndPlayFile(filePath);
}

void MainPresenter::OnPlayPauseRequested()
{
    if (!m_model.HasMedia())
    {
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

void MainPresenter::OnRepeatToggled()
{
    bool nextRepeat = !m_model.IsRepeatEnabled();
    m_model.SetRepeat(nextRepeat);
    m_view.SetRepeatChecked(nextRepeat);
    UpdateView();
}

void MainPresenter::LoadAndPlayFile(const std::wstring& filePath)
{
    if (m_model.OpenFile(filePath))
    {
        m_model.Play();
        UpdateView();
    }
}

void MainPresenter::UpdateView()
{
    if (!m_model.HasMedia())
    {
        m_view.SetWindowTitle(L"NextVideoPlayer");
        m_view.SetStatusText(L"동영상 파일을 열거나 창으로 드래그하세요.");
        return;
    }

    std::filesystem::path path(m_model.GetFilePath());
    std::wstring fileName = path.filename().wstring();

    std::wstring stateLabel;
    switch (m_model.GetState())
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

    std::wstring repeatLabel = m_model.IsRepeatEnabled() ? L" [반복 ON]" : L" [반복 OFF]";
    m_view.SetWindowTitle(L"NextVideoPlayer - " + fileName);
    m_view.SetStatusText(fileName + L" (" + stateLabel + L")" + repeatLabel);
}

