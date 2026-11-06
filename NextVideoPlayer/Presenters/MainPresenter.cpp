#include "Presenters/MainPresenter.h"

MainPresenter::MainPresenter(IMainView& shell, VideoPlayerPresenter& videoPlayer)
    : m_shell(shell), m_videoPlayer(videoPlayer)
{
}

MainPresenter::~MainPresenter()
{
    m_shell.SetOnTimer({});
    m_shell.SetOnCommand({});
    m_shell.SetOnKeyDown({});
    m_shell.SetOnMouseWheel({});
    m_shell.SetOnFilesDropped({});
    m_shell.SetOnFullscreenChanged({});
}

bool MainPresenter::Initialize()
{
    if (!m_videoPlayer.Initialize())
    {
        return false;
    }

    m_shell.SetOnCommand([this](int commandId) { m_videoPlayer.HandleShellCommand(commandId); });
    m_shell.SetOnKeyDown([this](WPARAM key) { m_videoPlayer.HandleShellKeyDown(key); });
    m_shell.SetOnMouseWheel([this](int delta) { m_videoPlayer.HandleShellMouseWheel(delta); });
    m_shell.SetOnFilesDropped([this](HDROP drop) { m_videoPlayer.HandleShellDropFiles(drop); });
    m_shell.SetOnFullscreenChanged([this](bool fullscreen) { m_videoPlayer.OnShellFullscreenChanged(fullscreen); });
    return true;
}

void MainPresenter::Tick()
{
    m_videoPlayer.Tick();
}