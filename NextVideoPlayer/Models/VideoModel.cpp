#include "Models/VideoModel.h"

bool VideoModel::OpenFile(const std::wstring& filePath)
{
    if (filePath.empty())
    {
        return false;
    }

    DWORD attributes = GetFileAttributesW(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        return false;
    }

    m_filePath = filePath;
    m_state = PlaybackState::Stopped;
    return true;
}

void VideoModel::Play()
{
    if (HasMedia())
    {
        m_state = PlaybackState::Playing;
    }
}

void VideoModel::Pause()
{
    if (m_state == PlaybackState::Playing)
    {
        m_state = PlaybackState::Paused;
    }
}

void VideoModel::Stop()
{
    m_state = PlaybackState::Stopped;
}

void VideoModel::SetRepeat(bool enabled)
{
    m_isRepeatEnabled = enabled;
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

