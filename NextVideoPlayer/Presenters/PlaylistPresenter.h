#pragma once
#include "Models/PlaylistModel.h"
#include "Models/IVideoModel.h"
#include "Views/IPlaylistView.h"
#include <limits>

class PlaylistPresenter
{
public:
    PlaylistPresenter(IPlaylistView& view, PlaylistModel& model, IVideoModel& video);
    ~PlaylistPresenter();
    bool Initialize(HINSTANCE instance, HWND owner);
    bool OpenFile(const std::wstring& path);
    bool OpenFiles(const std::vector<std::wstring>& paths);
    void AddFile(const std::wstring& path);
    void AddFiles(const std::vector<std::wstring>& paths);
    bool PlaySelection(const std::vector<int>& indices);
    void RemoveSelection(const std::vector<int>& indices);
    bool PlayCurrentOrFirst();
    bool PlayIndex(int index);
    bool PlayNext();
    bool PlayPrevious();
    void Toggle();
    bool IsVisible() const { return m_view.IsVisible(); }
    void Refresh();
    void SetOnChanged(std::function<void()> handler) { m_onChanged = std::move(handler); }

private:
    void Notify();
    IPlaylistView& m_view;
    PlaylistModel& m_model;
    IVideoModel& m_video;
    std::uint64_t m_renderedRevision = std::numeric_limits<std::uint64_t>::max();
    std::function<void()> m_onChanged;
};
