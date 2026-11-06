#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PlaylistModel
{
public:
    void Add(const std::wstring& filePath);
    void Remove(int index);
    void Clear();
    void Select(const std::wstring& filePath);
    bool SetPlaybackFiles(const std::vector<std::wstring>& files);
    int GetNextIndex(bool repeat = false) const;
    int GetPreviousIndex() const;
    int GetCurrentIndex() const { return m_currentIndex; }
    const std::vector<std::wstring>& GetItems() const { return m_items; }
    std::uint64_t GetRevision() const { return m_revision; }

private:
    int IndexOf(const std::wstring& filePath) const;
    std::vector<std::wstring> m_items;
    std::vector<std::wstring> m_playbackFiles;
    int m_playbackPosition = -1;
    bool m_hasPlaybackQueue = false;
    int m_currentIndex = -1;
    std::uint64_t m_revision = 0;
};
