#include "Services/Api/PlayerControlService.h"
#include "Services/Api/TextEncoding.h"
#include <filesystem>
#include <algorithm>
#include <set>

PlayerControlService::PlayerControlService(IVideoModel &video, PlaylistModel &playlist, PlaylistPresenter &presenter,
                                           IMainView &window, AppConfig &config, std::function<void(bool)> changed)
    : m_video(video), m_playlist(playlist), m_playlistPresenter(presenter), m_window(window), m_config(config),
      m_changed(std::move(changed))
{
    GUID id{};
    CoCreateGuid(&id);
    wchar_t text[40]{};
    StringFromGUID2(id, text, 40);
    m_instanceId = Api::ToUtf8(text);
}
Api::Result PlayerControlService::Execute(const Api::Command &command)
{
    using namespace Api;
    auto fail = [](const char *code, const char *text) { return Result{false, false, code, text}; };
    std::vector<std::wstring> paths;
    if (command.files)
    {
        for (const auto &file : *command.files)
        {
            const auto wide = ToWide(file);
            const std::filesystem::path path(wide);
            std::error_code error;
            // Limit control requests to existing local media files.
            if (!path.is_absolute() || wide.rfind(L"\\\\", 0) == 0 || !std::filesystem::is_regular_file(path, error))
                return fail("invalid_file", "files must contain existing absolute local file paths");
            paths.push_back(wide);
        }
    }
    if (command.playlistIndex)
    {
        std::set<std::wstring> unique(m_playlist.GetItems().begin(), m_playlist.GetItems().end());
        for (const auto &path : paths)
            unique.insert(path);
        if (static_cast<std::size_t>(*command.playlistIndex) >= unique.size())
            return fail("invalid_index", "playlistIndex is outside the playlist");
    }
    const bool opensMedia = command.files.has_value() || command.playlistIndex.has_value();
    const bool requestsPlayback = command.playback && *command.playback != "stopped";
    if (command.positionMs && !opensMedia && (!m_video.HasMedia() || m_video.GetState() == PlaybackState::Stopped) &&
        !requestsPlayback)
        return fail("no_media", "Open or play media before seeking");
    if (command.playback && *command.playback != "stopped" && !opensMedia && !m_video.HasMedia() &&
        m_playlist.GetItems().empty())
        return fail("no_media", "No media is selected");

    if (opensMedia || command.positionMs || (command.playback && *command.playback == "stopped"))
        m_pendingPosition.reset();
    if (!paths.empty() && !m_playlistPresenter.OpenFiles(paths))
        return fail("open_failed", "Could not open the selected media");
    if (command.playlistIndex && !m_playlistPresenter.PlayIndex(*command.playlistIndex))
        return fail("open_failed", "Could not open playlist item");
    bool persist = false;
    if (command.speed && *command.speed != m_video.GetSpeed())
    {
        m_video.SetSpeed(*command.speed);
        persist = true;
    }
    if (command.volume && *command.volume != m_video.GetVolume())
    {
        m_video.SetVolume(*command.volume);
        persist = true;
    }
    if (command.muted && *command.muted != m_video.IsMuted())
    {
        m_video.SetMuted(*command.muted);
        persist = true;
    }
    if (command.repeat && *command.repeat != m_video.IsRepeatEnabled())
    {
        m_video.SetRepeat(*command.repeat);
        persist = true;
    }
    if (command.topMost && *command.topMost != m_config.topMost)
    {
        m_config.topMost = *command.topMost;
        m_window.SetTopMost(*command.topMost);
        persist = true;
    }
    if (command.fullscreen && *command.fullscreen != m_window.IsFullscreen())
        m_window.ToggleFullscreen();
    if (command.playback)
    {
        if (*command.playback == "stopped")
            m_video.Stop();
        else
        {
            if (!m_video.HasMedia())
            {
                if (!m_playlistPresenter.PlayCurrentOrFirst())
                    return fail("no_media", "No media is selected");
            }
            m_video.Play();
            if (*command.playback == "paused")
                m_video.Pause();
        }
    }
    if (command.positionMs && m_video.GetState() != PlaybackState::Stopped)
    {
        if (m_video.IsMediaReady())
            m_video.SeekAbsolute(*command.positionMs / 1000.0);
        else
        {
            m_pendingPosition = *command.positionMs / 1000.0;
            m_pendingFile = ToUtf8(m_video.GetFilePath());
            m_pendingGeneration = m_video.GetMediaGeneration();
        }
    }
    if (m_changed && command.HasChanges())
        m_changed(persist);
    return {true, opensMedia || m_pendingPosition.has_value(), {}, {}};
}
void PlayerControlService::Tick()
{
    if (!m_pendingPosition)
        return;
    if (m_video.GetMediaGeneration() != m_pendingGeneration || Api::ToUtf8(m_video.GetFilePath()) != m_pendingFile ||
        m_video.GetState() == PlaybackState::Stopped || !m_video.GetMediaError().empty())
    {
        m_pendingPosition.reset();
        return;
    }
    if (m_video.IsMediaReady())
    {
        m_video.SeekAbsolute(*m_pendingPosition);
        m_pendingPosition.reset();
        if (m_changed)
            m_changed(false);
    }
}
Api::State PlayerControlService::Capture(bool includeFiles) const
{
    Api::State state;
    state.instanceId = m_instanceId;
    const auto playback = m_video.GetState();
    state.playback = playback == PlaybackState::Playing  ? "playing"
                     : playback == PlaybackState::Paused ? "paused"
                                                         : "stopped";
    state.file = Api::ToUtf8(m_video.GetFilePath());
    state.ready = m_video.IsMediaReady();
    state.loading = m_video.IsLoading();
    state.mediaError = m_video.GetMediaError();
    state.positionMs = m_video.GetPosition() * 1000.0;
    state.durationMs = m_video.GetDuration() * 1000.0;
    state.speed = m_video.GetSpeed();
    state.volume = m_video.GetVolume();
    state.muted = m_video.IsMuted();
    state.repeat = m_video.IsRepeatEnabled();
    state.fullscreen = m_window.IsFullscreen();
    state.topMost = m_config.topMost;
    state.playlistIndex = m_playlist.GetCurrentIndex();
    state.playlistRevision = m_playlist.GetRevision();
    state.playlistCount = static_cast<int>(m_playlist.GetItems().size());
    if (includeFiles)
        for (const auto &path : m_playlist.GetItems())
            state.files.push_back(Api::ToUtf8(path));
    state.timestampMs = Api::UnixMilliseconds();
    return state;
}
