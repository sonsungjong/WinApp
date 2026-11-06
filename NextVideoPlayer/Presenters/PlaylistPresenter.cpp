#include "Presenters/PlaylistPresenter.h"
#include <algorithm>

PlaylistPresenter::PlaylistPresenter(IPlaylistView& view, PlaylistModel& model, IVideoModel& video)
    : m_view(view), m_model(model), m_video(video)
{
    m_view.SetOnPlay([this](const std::vector<int>& indices) { PlaySelection(indices); });
    m_view.SetOnAdd([this]() { AddFiles(m_view.PromptOpenFiles()); });
    m_view.SetOnRemove([this](const std::vector<int>& indices) { RemoveSelection(indices); });
    m_view.SetOnClear([this]() { m_model.Clear(); Notify(); });
    m_view.SetOnVisibilityChanged([this]() { Notify(); });
    m_video.SetOnPlaybackEnded([this]() { if (!PlayNext()) Notify(); });
}

PlaylistPresenter::~PlaylistPresenter()
{
    m_video.SetOnPlaybackEnded({});
    m_view.SetOnPlay({});
    m_view.SetOnAdd({});
    m_view.SetOnRemove({});
    m_view.SetOnClear({});
    m_view.SetOnVisibilityChanged({});
}

bool PlaylistPresenter::Initialize(HINSTANCE instance, HWND owner)
{
    if (!m_view.Initialize(instance, owner)) return false;
    Refresh();
    return true;
}

bool PlaylistPresenter::OpenFile(const std::wstring& path)
{
    return OpenFiles({ path });
}

bool PlaylistPresenter::OpenFiles(const std::vector<std::wstring>& paths)
{
    if (paths.empty()) return false;
    for (const auto& path : paths) m_model.Add(path);
    if (m_model.SetPlaybackFiles(paths) && PlayIndex(m_model.GetNextIndex())) return true;
    Notify();
    return false;
}

void PlaylistPresenter::AddFile(const std::wstring& path)
{
    AddFiles({ path });
}

void PlaylistPresenter::AddFiles(const std::vector<std::wstring>& paths)
{
    if (paths.empty()) return;
    for (const auto& path : paths) m_model.Add(path);
    Notify();
}

bool PlaylistPresenter::PlaySelection(const std::vector<int>& indices)
{
    if (indices.empty())
        return m_model.GetItems().empty() ? OpenFiles(m_view.PromptOpenFiles()) : PlayCurrentOrFirst();

    auto ordered = indices;
    std::sort(ordered.begin(), ordered.end());
    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
    std::vector<std::wstring> files;
    const auto& items = m_model.GetItems();
    for (int index : ordered)
        if (index >= 0 && index < static_cast<int>(items.size())) files.push_back(items[index]);
    return m_model.SetPlaybackFiles(files) && PlayIndex(m_model.GetNextIndex());
}

void PlaylistPresenter::RemoveSelection(const std::vector<int>& indices)
{
    if (indices.empty()) return;
    auto ordered = indices;
    std::sort(ordered.begin(), ordered.end());
    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) m_model.Remove(*it);
    Notify();
}

bool PlaylistPresenter::PlayCurrentOrFirst()
{
    if (m_model.GetItems().empty()) return false;
    int index = m_model.GetCurrentIndex();
    if (index < 0) index = m_model.GetNextIndex();
    return PlayIndex(index < 0 ? 0 : index);
}

bool PlaylistPresenter::PlayIndex(int index)
{
    const auto& items = m_model.GetItems();
    if (index < 0 || index >= static_cast<int>(items.size())) return false;
    const std::wstring path = items[index];
    if (!m_video.OpenFile(path)) return false;
    m_model.Select(path);
    Notify();
    return true;
}

bool PlaylistPresenter::PlayNext() { return PlayIndex(m_model.GetNextIndex(m_video.IsRepeatEnabled())); }
bool PlaylistPresenter::PlayPrevious() { return PlayIndex(m_model.GetPreviousIndex()); }

void PlaylistPresenter::Toggle()
{
    Refresh();
    m_view.SetVisible(!m_view.IsVisible());
}

void PlaylistPresenter::Refresh()
{
    if (m_renderedRevision != m_model.GetRevision())
    {
        m_view.Render(m_model.GetItems(), m_model.GetCurrentIndex());
        m_renderedRevision = m_model.GetRevision();
    }
}

void PlaylistPresenter::Notify()
{
    Refresh();
    if (m_onChanged) m_onChanged();
}
