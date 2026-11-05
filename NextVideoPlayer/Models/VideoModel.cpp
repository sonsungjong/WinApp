#include "Models/VideoModel.h"
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
    mpv_set_option_string(m_mpv, "keep-open", "yes");
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

    return true;
}

bool VideoModel::OpenFile(const std::wstring& filePath)
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
        OpenFile(m_filePath);
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
            if (endFile && endFile->reason == MPV_END_FILE_REASON_EOF && !m_isRepeatEnabled)
            {
                m_state = PlaybackState::Stopped;
            }
        }
    }
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
