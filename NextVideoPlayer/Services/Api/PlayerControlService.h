#pragma once
#include "Models/ConfigService.h"
#include "Models/IVideoModel.h"
#include "Models/PlaylistModel.h"
#include "Presenters/PlaylistPresenter.h"
#include "Views/IMainView.h"
#include "Services/Api/ApiTypes.h"

// UI-thread application facade. No WebSocket/library types cross this interface.
class PlayerControlService
{
  public:
    PlayerControlService(IVideoModel &, PlaylistModel &, PlaylistPresenter &, IMainView &, AppConfig &,
                         std::function<void(bool)> changed);
    Api::Result Execute(const Api::Command &);
    Api::State Capture(bool includeFiles = false) const;
    void Tick();

  private:
    IVideoModel &m_video;
    PlaylistModel &m_playlist;
    PlaylistPresenter &m_playlistPresenter;
    IMainView &m_window;
    AppConfig &m_config;
    std::function<void(bool)> m_changed;
    std::optional<double> m_pendingPosition;
    std::string m_pendingFile;
    std::uint64_t m_pendingGeneration = 0;
    std::string m_instanceId;
};
