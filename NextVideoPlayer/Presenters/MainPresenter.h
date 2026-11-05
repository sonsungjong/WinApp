#pragma once

#include "Models/IVideoModel.h"
#include "Views/IMainView.h"

class MainPresenter
{
public:
    MainPresenter(IMainView& view, IVideoModel& model);
    ~MainPresenter() = default;

    bool Initialize();
    void Tick();
    bool IsPlaying() const;

private:
    void OnOpenFileRequested();
    void OnFileDropped(const std::wstring& filePath);
    void OnPlayPauseRequested();
    void OnStopRequested();
    void OnRepeatToggled();
    void LoadAndPlayFile(const std::wstring& filePath);
    void UpdateView();

    IMainView& m_view;
    IVideoModel& m_model;
};
