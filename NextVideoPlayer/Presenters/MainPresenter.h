#pragma once
#include "Presenters/VideoPlayerPresenter.h"
#include "Views/IMainView.h"

class MainPresenter
{
public:
    MainPresenter(IMainView& shell, VideoPlayerPresenter& videoPlayer);
    ~MainPresenter();
    bool Initialize();
    void Tick();

private:
    IMainView& m_shell;
    VideoPlayerPresenter& m_videoPlayer;
};