#include "Models/PlaylistModel.h"
#include <algorithm>

void PlaylistModel::Add(const std::wstring& filePath)
{
    if (!filePath.empty() && std::find(m_items.begin(), m_items.end(), filePath) == m_items.end())
    {
        m_items.push_back(filePath);
        if (m_hasPlaybackQueue) m_playbackFiles.push_back(filePath);
        ++m_revision;
    }
}

void PlaylistModel::Remove(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) return;
    const std::wstring removed = m_items[index];
    m_items.erase(m_items.begin() + index);
    if (index == m_currentIndex) m_currentIndex = -1;
    else if (index < m_currentIndex) --m_currentIndex;
    for (int i = static_cast<int>(m_playbackFiles.size()) - 1; i >= 0; --i)
    {
        if (m_playbackFiles[i] != removed) continue;
        m_playbackFiles.erase(m_playbackFiles.begin() + i);
        if (i <= m_playbackPosition) --m_playbackPosition;
    }
    ++m_revision;
}

void PlaylistModel::Clear()
{
    if (m_items.empty()) return;
    m_items.clear();
    m_playbackFiles.clear();
    m_playbackPosition = -1;
    m_hasPlaybackQueue = false;
    m_currentIndex = -1;
    ++m_revision;
}

void PlaylistModel::Select(const std::wstring& filePath)
{
    Add(filePath);
    auto it = std::find(m_items.begin(), m_items.end(), filePath);
    int index = it == m_items.end() ? -1 : static_cast<int>(it - m_items.begin());
    if (index != m_currentIndex)
    {
        m_currentIndex = index;
        ++m_revision;
    }
    auto playing = std::find(m_playbackFiles.begin(), m_playbackFiles.end(), filePath);
    if (!m_hasPlaybackQueue || playing == m_playbackFiles.end())
    {
        m_playbackFiles = m_items;
        m_hasPlaybackQueue = true;
        playing = std::find(m_playbackFiles.begin(), m_playbackFiles.end(), filePath);
    }
    m_playbackPosition = playing == m_playbackFiles.end() ? -1 : static_cast<int>(playing - m_playbackFiles.begin());
}

int PlaylistModel::IndexOf(const std::wstring& filePath) const
{
    auto it = std::find(m_items.begin(), m_items.end(), filePath);
    return it == m_items.end() ? -1 : static_cast<int>(it - m_items.begin());
}

bool PlaylistModel::SetPlaybackFiles(const std::vector<std::wstring>& files)
{
    std::vector<std::wstring> queue;
    for (const auto& path : files)
        if (IndexOf(path) >= 0 && std::find(queue.begin(), queue.end(), path) == queue.end())
            queue.push_back(path);
    if (queue.empty()) return false;
    m_playbackFiles = std::move(queue);
    m_playbackPosition = -1;
    m_hasPlaybackQueue = true;
    return true;
}

int PlaylistModel::GetNextIndex(bool repeat) const
{
    if (m_hasPlaybackQueue)
    {
        if (m_playbackFiles.empty()) return -1;
        int next = m_playbackPosition + 1;
        if (next >= static_cast<int>(m_playbackFiles.size()))
            return repeat ? IndexOf(m_playbackFiles.front()) : -1;
        return IndexOf(m_playbackFiles[next]);
    }
    int next = m_currentIndex + 1;
    return next < static_cast<int>(m_items.size()) ? next : (repeat && !m_items.empty() ? 0 : -1);
}

int PlaylistModel::GetPreviousIndex() const
{
    if (m_hasPlaybackQueue)
        return m_playbackFiles.empty() ? -1 : IndexOf(m_playbackFiles[std::max(0, m_playbackPosition - 1)]);
    return m_items.empty() ? -1 : std::max(0, m_currentIndex - 1);
}
