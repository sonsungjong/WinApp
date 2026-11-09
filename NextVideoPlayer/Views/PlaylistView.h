#pragma once

#include "Views/IPlaylistView.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <unordered_map>

class PlaylistView : public IPlaylistView
{
public:
    explicit PlaylistView(HFONT font = nullptr);
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

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    void Draw();

    ID2D1Bitmap* GetOrCreateThumbnail(const std::wstring& path);
    void RebuildFilteredItems();
    std::vector<int> GetSelectedIndices() const;
    void ClampScroll();
    void ScrollToItem(int visibleIdx);

    D2D1_RECT_F GetTitleBarButtonRect(int index) const; // 0: min, 1: max, 2: close
    D2D1_RECT_F GetSearchBoxRect() const;
    D2D1_RECT_F GetListRect() const;
    D2D1_RECT_F GetCardRect(int visibleIdx) const;
    D2D1_RECT_F GetCardPlayButtonRect(const D2D1_RECT_F& cardRect) const;
    D2D1_RECT_F GetCardDeleteButtonRect(const D2D1_RECT_F& cardRect) const;
    D2D1_RECT_F GetFooterButtonRect(int btnIndex) const; // 0: Add, 1: Play, 2: Clear
    D2D1_RECT_F GetScrollTrackRect() const;
    D2D1_RECT_F GetScrollThumbRect() const;
    float GetTotalContentHeight() const;
    float GetMaxScroll() const;

    int HitTestVisibleCard(float x, float y, int* outInlineAction = nullptr) const;
    int HitTestFooterButton(float x, float y) const;
    int HitTestTitleBarButton(float x, float y) const;

    HWND m_hwnd = nullptr;
    HWND m_owner = nullptr;
    HINSTANCE m_instance = nullptr;

    std::vector<std::wstring> m_items;
    std::vector<int> m_filteredIndices; // maps visible row -> index in m_items
    std::wstring m_searchQuery;
    bool m_searchFocused = false;

    int m_currentIndex = -1;
    std::vector<int> m_selectedIndices; // indices in m_items
    int m_anchorIndex = -1;

    int m_hoveredVisibleIdx = -1;
    int m_hoveredInlineAction = 0; // 0: none, 1: play, 2: delete
    int m_hoveredFooterBtn = -1;
    int m_pressedFooterBtn = -1;
    int m_hoveredTitleBtn = -1;
    int m_pressedTitleBtn = -1;
    bool m_trackingMouse = false;

    float m_scrollOffset = 0.0f;
    bool m_isDraggingScroll = false;
    float m_dragStartY = 0.0f;
    float m_dragStartScrollOffset = 0.0f;
    bool m_isHoveringScrollThumb = false;

    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> m_thumbnailCache;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_captionFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_badgeFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_searchFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_itemFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_subFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnBoldFormat;

    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_bgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_windowBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_separatorBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardActiveBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cyanBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cyanGlowBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_amberBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_badgeBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_activeBadgeBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_subTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_darkTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnPressedBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_closeHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_scrollTrackBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_scrollThumbBrush;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_addBtnGradBrush;

    std::function<void(const std::vector<int>&)> m_onPlay;
    std::function<void()> m_onAdd;
    std::function<void(const std::vector<int>&)> m_onRemove;
    std::function<void()> m_onClear;
    std::function<void()> m_onVisibilityChanged;
};
