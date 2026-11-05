#include "Models/VideoModel.h"
#include <algorithm>
#include <clocale>
#include <mpv/client.h>

#pragma comment(lib, "libmpv.lib")

VideoModel::~VideoModel()
{
    if (m_mpv)
    {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

bool VideoModel::Initialize(HWND videoHwnd)
{
    if (m_mpv)
    {
        return true;
    }

    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv)
    {
        return false;
    }

    int64_t wid = reinterpret_cast<int64_t>(videoHwnd);
    mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
    mpv_set_option_string(m_mpv, "vo", "gpu");
    mpv_set_option_string(m_mpv, "gpu-api", "d3d11");
    mpv_set_option_string(m_mpv, "hwdec", "auto");
    mpv_set_option_string(m_mpv, "keep-open", "no");
    mpv_set_option_string(m_mpv, "loop-file", m_isRepeatEnabled ? "inf" : "no");
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");
    mpv_set_option_string(m_mpv, "sub-codepage", "auto");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");

    if (mpv_initialize(m_mpv) < 0)
    {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    SetVolume(m_volume);
    SetMuted(m_isMuted);
    SetSpeed(m_speed);
    SetSubtitleVisible(m_subtitleVisible);
    SetSubtitleScale(m_subtitleScale);

    return true;
}

bool VideoModel::OpenFile(const std::wstring& filePath)
{
    if (!LoadFileInternal(filePath))
    {
        return false;
    }

    auto it = std::find(m_playlist.begin(), m_playlist.end(), filePath);
    if (it == m_playlist.end())
    {
        m_playlist.push_back(filePath);
        m_currentPlaylistIndex = static_cast<int>(m_playlist.size()) - 1;
    }
    else
    {
        m_currentPlaylistIndex = static_cast<int>(std::distance(m_playlist.begin(), it));
    }

    return true;
}

bool VideoModel::LoadFileInternal(const std::wstring& filePath)
{
    if (!m_mpv || filePath.empty())
    {
        return false;
    }

    DWORD attributes = GetFileAttributesW(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        return false;
    }

    ClearABLoop();
    m_subtitleDelay = 0.0;
    mpv_set_property(m_mpv, "sub-delay", MPV_FORMAT_DOUBLE, &m_subtitleDelay);

    std::string utf8Path = ToUtf8(filePath);
    const char* cmd[] = { "loadfile", utf8Path.c_str(), "replace", nullptr };
    if (mpv_command(m_mpv, cmd) < 0)
    {
        return false;
    }

    int pauseFlag = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pauseFlag);

    m_filePath = filePath;
    m_state = PlaybackState::Playing;
    return true;
}

void VideoModel::Play()
{
    if (!m_mpv || !HasMedia())
    {
        return;
    }

    if (m_state == PlaybackState::Stopped)
    {
        LoadFileInternal(m_filePath);
        return;
    }

    int pauseFlag = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_state = PlaybackState::Playing;
}

void VideoModel::Pause()
{
    if (!m_mpv || m_state != PlaybackState::Playing)
    {
        return;
    }

    int pauseFlag = 1;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_state = PlaybackState::Paused;
}

void VideoModel::Stop()
{
    if (m_mpv)
    {
        const char* cmd[] = { "stop", nullptr };
        mpv_command(m_mpv, cmd);
    }

    m_state = PlaybackState::Stopped;
}

void VideoModel::ProcessEvents()
{
    while (m_mpv)
    {
        mpv_event* event = mpv_wait_event(m_mpv, 0.0);
        if (!event || event->event_id == MPV_EVENT_NONE)
        {
            break;
        }

        if (event->event_id == MPV_EVENT_END_FILE)
        {
            auto* endFile = static_cast<mpv_event_end_file*>(event->data);
            if (endFile && endFile->reason == MPV_END_FILE_REASON_EOF)
            {
                if (m_isRepeatEnabled)
                {
                    LoadFileInternal(m_filePath);
                }
                else if (m_currentPlaylistIndex + 1 < static_cast<int>(m_playlist.size()))
                {
                    PlayNext();
                }
                else
                {
                    m_state = PlaybackState::Stopped;
                }
            }
        }
    }
}

void VideoModel::SeekAbsolute(double seconds)
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return;
    }

    double duration = GetDuration();
    if (duration > 0.0)
    {
        seconds = std::clamp(seconds, 0.0, duration);
    }
    else if (seconds < 0.0)
    {
        seconds = 0.0;
    }

    mpv_set_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &seconds);
}

void VideoModel::SeekRelative(double deltaSeconds)
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return;
    }

    double target = GetPosition() + deltaSeconds;
    SeekAbsolute(target);
}

double VideoModel::GetPosition() const
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return 0.0;
    }

    double pos = 0.0;
    if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0)
    {
        return 0.0;
    }
    return std::max(0.0, pos);
}

double VideoModel::GetDuration() const
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return 0.0;
    }

    double duration = 0.0;
    if (mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &duration) < 0)
    {
        return 0.0;
    }
    return std::max(0.0, duration);
}

void VideoModel::SetVolume(int volume)
{
    m_volume = std::clamp(volume, 0, 100);
    if (m_mpv)
    {
        double vol = static_cast<double>(m_volume);
        mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &vol);
    }
}

int VideoModel::GetVolume() const
{
    return m_volume;
}

void VideoModel::SetMuted(bool muted)
{
    m_isMuted = muted;
    if (m_mpv)
    {
        int flag = muted ? 1 : 0;
        mpv_set_property(m_mpv, "mute", MPV_FORMAT_FLAG, &flag);
    }
}

bool VideoModel::IsMuted() const
{
    return m_isMuted;
}

void VideoModel::SetSpeed(double speed)
{
    m_speed = std::clamp(speed, 0.25, 4.0);
    if (m_mpv)
    {
        mpv_set_property(m_mpv, "speed", MPV_FORMAT_DOUBLE, &m_speed);
    }
}

double VideoModel::GetSpeed() const
{
    return m_speed;
}

void VideoModel::SetRepeat(bool enabled)
{
    m_isRepeatEnabled = enabled;
    if (m_mpv)
    {
        mpv_set_property_string(m_mpv, "loop-file", enabled ? "inf" : "no");
    }
}

bool VideoModel::IsRepeatEnabled() const
{
    return m_isRepeatEnabled;
}

void VideoModel::SetABLoopA()
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return;
    }

    m_abLoopA = GetPosition();
    mpv_set_property(m_mpv, "ab-loop-a", MPV_FORMAT_DOUBLE, &m_abLoopA);
}

void VideoModel::SetABLoopB()
{
    if (!m_mpv || m_state == PlaybackState::Stopped)
    {
        return;
    }

    m_abLoopB = GetPosition();
    mpv_set_property(m_mpv, "ab-loop-b", MPV_FORMAT_DOUBLE, &m_abLoopB);
}

void VideoModel::ClearABLoop()
{
    m_abLoopA = -1.0;
    m_abLoopB = -1.0;
    if (m_mpv)
    {
        mpv_set_property_string(m_mpv, "ab-loop-a", "no");
        mpv_set_property_string(m_mpv, "ab-loop-b", "no");
    }
}

double VideoModel::GetABLoopA() const
{
    return m_abLoopA;
}

double VideoModel::GetABLoopB() const
{
    return m_abLoopB;
}

bool VideoModel::LoadSubtitleFile(const std::wstring& subFilePath)
{
    if (!m_mpv || !HasMedia() || subFilePath.empty())
    {
        return false;
    }

    std::string utf8Path = ToUtf8(subFilePath);
    const char* cmd[] = { "sub-add", utf8Path.c_str(), "select", nullptr };
    return mpv_command(m_mpv, cmd) >= 0;
}

void VideoModel::SetSubtitleVisible(bool visible)
{
    m_subtitleVisible = visible;
    if (m_mpv)
    {
        int flag = visible ? 1 : 0;
        mpv_set_property(m_mpv, "sub-visibility", MPV_FORMAT_FLAG, &flag);
    }
}

bool VideoModel::IsSubtitleVisible() const
{
    return m_subtitleVisible;
}

void VideoModel::AdjustSubtitleDelay(double deltaSeconds)
{
    m_subtitleDelay += deltaSeconds;
    if (m_mpv)
    {
        mpv_set_property(m_mpv, "sub-delay", MPV_FORMAT_DOUBLE, &m_subtitleDelay);
    }
}

double VideoModel::GetSubtitleDelay() const
{
    return m_subtitleDelay;
}

void VideoModel::SetSubtitleScale(double scale)
{
    m_subtitleScale = std::clamp(scale, 0.3, 3.0);
    if (m_mpv)
    {
        mpv_set_property(m_mpv, "sub-scale", MPV_FORMAT_DOUBLE, &m_subtitleScale);
    }
}

double VideoModel::GetSubtitleScale() const
{
    return m_subtitleScale;
}

void VideoModel::AdjustSubtitlePos(int deltaPos)
{
    m_subtitlePos = std::clamp(m_subtitlePos + deltaPos, 10, 150);
    if (m_mpv)
    {
        int64_t pos = static_cast<int64_t>(m_subtitlePos);
        mpv_set_property(m_mpv, "sub-pos", MPV_FORMAT_INT64, &pos);
    }
}

bool VideoModel::TakeScreenshot(const std::wstring& filePath)
{
    if (!m_mpv || m_state == PlaybackState::Stopped || filePath.empty())
    {
        return false;
    }

    std::string utf8Path = ToUtf8(filePath);
    const char* cmd[] = { "screenshot-to-file", utf8Path.c_str(), "video", nullptr };
    return mpv_command(m_mpv, cmd) >= 0;
}

void VideoModel::AddToPlaylist(const std::wstring& filePath)
{
    if (filePath.empty())
    {
        return;
    }

    if (std::find(m_playlist.begin(), m_playlist.end(), filePath) == m_playlist.end())
    {
        m_playlist.push_back(filePath);
    }
}

void VideoModel::RemoveFromPlaylist(int index)
{
    if (index < 0 || index >= static_cast<int>(m_playlist.size()))
    {
        return;
    }

    m_playlist.erase(m_playlist.begin() + index);
    if (m_currentPlaylistIndex == index)
    {
        m_currentPlaylistIndex = -1;
    }
    else if (m_currentPlaylistIndex > index)
    {
        --m_currentPlaylistIndex;
    }
}

void VideoModel::ClearPlaylist()
{
    m_playlist.clear();
    m_currentPlaylistIndex = -1;
}

bool VideoModel::PlayPlaylistIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_playlist.size()))
    {
        return false;
    }

    if (!LoadFileInternal(m_playlist[index]))
    {
        return false;
    }

    m_currentPlaylistIndex = index;
    return true;
}

bool VideoModel::PlayNext()
{
    if (m_playlist.empty())
    {
        return false;
    }

    int nextIndex = m_currentPlaylistIndex + 1;
    if (nextIndex >= static_cast<int>(m_playlist.size()))
    {
        if (m_isRepeatEnabled)
        {
            nextIndex = 0;
        }
        else
        {
            return false;
        }
    }

    return PlayPlaylistIndex(nextIndex);
}

bool VideoModel::PlayPrevious()
{
    if (m_playlist.empty())
    {
        return false;
    }

    int prevIndex = m_currentPlaylistIndex - 1;
    if (prevIndex < 0)
    {
        prevIndex = 0;
    }

    return PlayPlaylistIndex(prevIndex);
}

const std::vector<std::wstring>& VideoModel::GetPlaylist() const
{
    return m_playlist;
}

int VideoModel::GetCurrentPlaylistIndex() const
{
    return m_currentPlaylistIndex;
}

PlaybackState VideoModel::GetState() const
{
    return m_state;
}

const std::wstring& VideoModel::GetFilePath() const
{
    return m_filePath;
}

bool VideoModel::HasMedia() const
{
    return !m_filePath.empty();
}

std::string VideoModel::ToUtf8(const std::wstring& wideStr)
{
    if (wideStr.empty())
    {
        return {};
    }

    int requiredSize = WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStr.data(),
        static_cast<int>(wideStr.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredSize <= 0)
    {
        return {};
    }

    std::string utf8Str(requiredSize, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wideStr.data(),
        static_cast<int>(wideStr.size()),
        utf8Str.data(),
        requiredSize,
        nullptr,
        nullptr);
    return utf8Str;
}
