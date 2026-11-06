#pragma once
#include "Views/IPlaylistView.h"

class PlaylistView : public IPlaylistView
{
public:
    explicit PlaylistView(HFONT font) : m_font(font) {}
    ~PlaylistView() override;
    bool Initialize(HINSTANCE instance, HWND owner) override;
    bool IsVisible() const override;
    void SetVisible(bool visible) override;
    void Render(const std::vector<std::wstring>& items, int currentIndex) override;
    std::vector<std::wstring> PromptOpenFiles() override;
    void SetOnPlay(std::function<void(const std::vector<int>&)> handler) override { m_onPlay = std::move(handler); }
    void SetOnAdd(std::function<void()> handler) override { m_onAdd = std::move(handler); }
    void SetOnRemove(std::function<void(const std::vector<int>&)> handler) override { m_onRemove = std::move(handler); }
    void SetOnClear(std::function<void()> handler) override { m_onClear = std::move(handler); }
    void SetOnVisibilityChanged(std::function<void()> handler) override { m_onVisibilityChanged = std::move(handler); }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    std::vector<int> GetSelectedIndices() const;
    HWND m_hwnd = nullptr;
    HWND m_list = nullptr;
    HFONT m_font = nullptr;
    std::vector<std::wstring> m_items;
    std::function<void(const std::vector<int>&)> m_onPlay;
    std::function<void()> m_onAdd;
    std::function<void(const std::vector<int>&)> m_onRemove;
    std::function<void()> m_onClear;
    std::function<void()> m_onVisibilityChanged;
};
