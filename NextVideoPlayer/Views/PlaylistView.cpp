#include "Views/PlaylistView.h"
#include "Views/FileDialogs.h"
#include "Views/FontHelper.h"
#include <dwmapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
    constexpr wchar_t kClassName[] = L"NextVideoPlayer_PlaylistWnd";
    constexpr int kDefaultWidth = 620;
    constexpr int kDefaultHeight = 530;

    constexpr float kTitleBarHeight = 38.0f;
    constexpr float kHeaderBottom = 94.0f;
    constexpr float kFooterHeight = 68.0f;
    constexpr float kCardHeight = 72.0f;
    constexpr float kCardMargin = 10.0f;
    constexpr float kSlotHeight = kCardHeight + kCardMargin;
    constexpr float kScrollbarWidth = 4.0f;
}

PlaylistView::PlaylistView(HFONT)
{
}

PlaylistView::~PlaylistView()
{
    if (m_hwnd && IsWindow(m_hwnd))
    {
        DestroyWindow(m_hwnd);
    }
}

bool PlaylistView::Initialize(HINSTANCE instance, HWND owner)
{
    m_instance = instance;
    m_owner = owner;

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    int posX = CW_USEDEFAULT;
    int posY = CW_USEDEFAULT;
    RECT rc{};
    if (owner && GetWindowRect(owner, &rc))
    {
        posX = rc.left + ((rc.right - rc.left) - kDefaultWidth) / 2;
        posY = rc.top + ((rc.bottom - rc.top) - kDefaultHeight) / 2;
    }

    m_hwnd = CreateWindowExW(
        0,
        kClassName,
        L"재생목록 (Playlist)",
        WS_POPUP | WS_THICKFRAME | WS_CLIPCHILDREN,
        posX,
        posY,
        kDefaultWidth,
        kDefaultHeight,
        owner,
        nullptr,
        instance,
        this);

    if (!m_hwnd)
    {
        return false;
    }

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hwnd, 20, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(m_hwnd, 19, &darkMode, sizeof(darkMode));
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hwnd, 33, &cornerPref, sizeof(cornerPref));

    return true;
}

bool PlaylistView::IsVisible() const
{
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void PlaylistView::SetVisible(bool visible)
{
    if (!m_hwnd || !IsWindow(m_hwnd))
    {
        return;
    }

    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    if (visible)
    {
        SetForegroundWindow(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    if (m_onVisibilityChanged)
    {
        m_onVisibilityChanged();
    }
}

void PlaylistView::RebuildFilteredItems()
{
    m_filteredIndices.clear();
    std::wstring queryLower = m_searchQuery;
    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::towlower);

    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
    {
        if (queryLower.empty())
        {
            m_filteredIndices.push_back(i);
        }
        else
        {
            std::wstring name = std::filesystem::path(m_items[i]).filename().wstring();
            std::transform(name.begin(), name.end(), name.begin(), ::towlower);
            if (name.find(queryLower) != std::wstring::npos)
            {
                m_filteredIndices.push_back(i);
            }
        }
    }
}

void PlaylistView::Render(const std::vector<std::wstring>& items, int currentIndex)
{
    m_items = items;
    m_currentIndex = currentIndex;
    RebuildFilteredItems();

    std::vector<int> validSelection;
    for (int idx : m_selectedIndices)
    {
        if (idx >= 0 && idx < static_cast<int>(m_items.size()))
        {
            validSelection.push_back(idx);
        }
    }
    m_selectedIndices = std::move(validSelection);

    if (m_selectedIndices.empty() && m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_items.size()))
    {
        m_selectedIndices.push_back(m_currentIndex);
    }

    ClampScroll();

    if (m_hwnd && IsWindow(m_hwnd))
    {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

std::vector<std::wstring> PlaylistView::PromptOpenFiles()
{
    return FileDialogs::OpenVideoFiles(m_hwnd);
}

std::vector<int> PlaylistView::GetSelectedIndices() const
{
    return m_selectedIndices;
}

void PlaylistView::ClampScroll()
{
    float maxScroll = GetMaxScroll();
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxScroll);
}

void PlaylistView::ScrollToItem(int visibleIdx)
{
    if (visibleIdx < 0 || visibleIdx >= static_cast<int>(m_filteredIndices.size()))
    {
        return;
    }

    D2D1_RECT_F rcList = GetListRect();
    float itemTop = visibleIdx * kSlotHeight;
    float itemBottom = itemTop + kCardHeight;
    float viewH = rcList.bottom - rcList.top;

    if (itemTop < m_scrollOffset)
    {
        m_scrollOffset = itemTop;
    }
    else if (itemBottom > m_scrollOffset + viewH)
    {
        m_scrollOffset = itemBottom - viewH;
    }

    ClampScroll();
}

ID2D1Bitmap* PlaylistView::GetOrCreateThumbnail(const std::wstring& path)
{
    if (!m_renderTarget || path.empty()) return nullptr;

    auto it = m_thumbnailCache.find(path);
    if (it != m_thumbnailCache.end())
    {
        return it->second.Get();
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap> d2dBitmap;
    Microsoft::WRL::ComPtr<IShellItemImageFactory> imageFactory;
    if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&imageFactory))) && imageFactory)
    {
        HBITMAP hbm = nullptr;
        SIZE sz{ 160, 92 };
        if (SUCCEEDED(imageFactory->GetImage(sz, SIIGBF_RESIZETOFIT | SIIGBF_THUMBNAILONLY, &hbm)) ||
            SUCCEEDED(imageFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hbm)))
        {
            if (hbm)
            {
                BITMAP bm{};
                if (GetObjectW(hbm, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0)
                {
                    BITMAPINFO bmi{};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = bm.bmWidth;
                    bmi.bmiHeader.biHeight = -bm.bmHeight; // top-down
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;

                    std::vector<uint32_t> pixels(static_cast<size_t>(bm.bmWidth) * bm.bmHeight);
                    HDC hdc = GetDC(nullptr);
                    if (GetDIBits(hdc, hbm, 0, bm.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS))
                    {
                        for (auto& px : pixels)
                        {
                            px |= 0xFF000000u; // ensure opaque alpha
                        }
                        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
                            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
                        m_renderTarget->CreateBitmap(
                            D2D1::SizeU(bm.bmWidth, bm.bmHeight),
                            pixels.data(),
                            bm.bmWidth * 4,
                            &props,
                            d2dBitmap.GetAddressOf());
                    }
                    ReleaseDC(nullptr, hdc);
                }
                DeleteObject(hbm);
            }
        }
    }

    m_thumbnailCache[path] = d2dBitmap;
    return d2dBitmap.Get();
}

HRESULT PlaylistView::CreateDeviceIndependentResources()
{
    if (m_d2dFactory && m_dwriteFactory) return S_OK;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) return hr;

    const wchar_t* korFontName = L"Malgun Gothic";
    FontHelper::CreateCustomFontCollection(m_dwriteFactory.Get(), m_customFontCollection, korFontName);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.0f, L"ko-kr", m_captionFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_captionFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        19.0f, L"ko-kr", m_titleFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.5f, L"ko-kr", m_badgeFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_badgeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_badgeFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.0f, L"ko-kr", m_searchFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_searchFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    m_searchFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"ko-kr", m_itemFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_itemFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    m_itemFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0f, L"ko-kr", m_subFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_subFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_subFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.5f, L"ko-kr", m_btnFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_btnFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_btnFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.5f, L"ko-kr", m_btnBoldFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_btnBoldFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_btnBoldFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    return S_OK;
}

HRESULT PlaylistView::CreateDeviceResources()
{
    if (m_renderTarget) return S_OK;

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        std::max(1L, rc.right - rc.left),
        std::max(1L, rc.bottom - rc.top));

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        m_renderTarget.GetAddressOf());
    if (FAILED(hr)) return hr;

    m_thumbnailCache.clear();

    if (FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x161922, 1.0f), m_bgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x323848, 1.0f), m_windowBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x232836, 1.0f), m_separatorBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1E222E, 1.0f), m_cardBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x262B3A, 1.0f), m_cardHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1B2B44, 1.0f), m_cardActiveBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2E3446, 1.0f), m_cardBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x38BDF8, 1.0f), m_cyanBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x00C8FF, 0.22f), m_cyanGlowBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF59E0B, 1.0f), m_amberBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x262B3A, 1.0f), m_badgeBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x0284C7, 1.0f), m_activeBadgeBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF4F7FC, 1.0f), m_textBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x8E96AA, 1.0f), m_subTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x071928, 1.0f), m_darkTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1C202B, 1.0f), m_btnBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x282E3E, 1.0f), m_btnHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x141720, 1.0f), m_btnPressedBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x363D50, 1.0f), m_btnBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xC42B1C, 1.0f), m_closeHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1C202B, 1.0f), m_scrollTrackBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x4E566E, 1.0f), m_scrollThumbBrush.GetAddressOf())))
    {
        DiscardDeviceResources();
        return hr;
    }

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x38BDF8, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x0096FF, 1.0f);
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCol;
    if (SUCCEEDED(m_renderTarget->CreateGradientStopCollection(stops, 2, &stopCol)))
    {
        m_renderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(24.0f, 0.0f), D2D1::Point2F(184.0f, 0.0f)),
            stopCol.Get(),
            m_addBtnGradBrush.GetAddressOf());
    }

    return S_OK;
}

void PlaylistView::DiscardDeviceResources()
{
    m_thumbnailCache.clear();
    m_addBtnGradBrush.Reset();
    m_scrollThumbBrush.Reset();
    m_scrollTrackBrush.Reset();
    m_closeHoverBrush.Reset();
    m_btnBorderBrush.Reset();
    m_btnPressedBrush.Reset();
    m_btnHoverBrush.Reset();
    m_btnBgBrush.Reset();
    m_darkTextBrush.Reset();
    m_subTextBrush.Reset();
    m_textBrush.Reset();
    m_activeBadgeBgBrush.Reset();
    m_badgeBgBrush.Reset();
    m_amberBrush.Reset();
    m_cyanGlowBrush.Reset();
    m_cyanBrush.Reset();
    m_cardBorderBrush.Reset();
    m_cardActiveBgBrush.Reset();
    m_cardHoverBrush.Reset();
    m_cardBgBrush.Reset();
    m_separatorBrush.Reset();
    m_windowBorderBrush.Reset();
    m_bgBrush.Reset();
    m_renderTarget.Reset();
}

D2D1_RECT_F PlaylistView::GetTitleBarButtonRect(int index) const
{
    if (!m_hwnd) return D2D1::RectF();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    float w = static_cast<float>(rc.right - rc.left);
    float btnW = 44.0f;
    float rightEdge = w - (2 - index) * btnW;
    return D2D1::RectF(rightEdge - btnW, 1.0f, rightEdge, kTitleBarHeight);
}

D2D1_RECT_F PlaylistView::GetSearchBoxRect() const
{
    if (!m_hwnd) return D2D1::RectF();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    float w = static_cast<float>(rc.right - rc.left);
    return D2D1::RectF(w - 224.0f, 48.0f, w - 24.0f, 82.0f);
}

D2D1_RECT_F PlaylistView::GetListRect() const
{
    if (!m_hwnd) return D2D1::RectF();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    float w = static_cast<float>(rc.right - rc.left);
    float h = static_cast<float>(rc.bottom - rc.top);
    return D2D1::RectF(24.0f, kHeaderBottom, w - 24.0f, h - kFooterHeight);
}

D2D1_RECT_F PlaylistView::GetCardRect(int visibleIdx) const
{
    D2D1_RECT_F rcList = GetListRect();
    bool hasScrollbar = (GetMaxScroll() > 0.0f);
    float rightEdge = hasScrollbar ? (rcList.right - 14.0f) : rcList.right;
    float top = rcList.top - m_scrollOffset + visibleIdx * kSlotHeight;
    return D2D1::RectF(rcList.left, top, rightEdge, top + kCardHeight);
}

D2D1_RECT_F PlaylistView::GetCardPlayButtonRect(const D2D1_RECT_F& cardRect) const
{
    float cy = (cardRect.top + cardRect.bottom) * 0.5f;
    return D2D1::RectF(cardRect.right - 84.0f, cy - 16.0f, cardRect.right - 52.0f, cy + 16.0f);
}

D2D1_RECT_F PlaylistView::GetCardDeleteButtonRect(const D2D1_RECT_F& cardRect) const
{
    float cy = (cardRect.top + cardRect.bottom) * 0.5f;
    return D2D1::RectF(cardRect.right - 44.0f, cy - 16.0f, cardRect.right - 12.0f, cy + 16.0f);
}

D2D1_RECT_F PlaylistView::GetFooterButtonRect(int btnIndex) const
{
    if (!m_hwnd) return D2D1::RectF();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    float h = static_cast<float>(rc.bottom - rc.top);
    float top = h - 52.0f;
    float bottom = h - 14.0f;

    if (btnIndex == 0) return D2D1::RectF(24.0f, top, 184.0f, bottom);
    if (btnIndex == 1) return D2D1::RectF(196.0f, top, 344.0f, bottom);
    if (btnIndex == 2) return D2D1::RectF(356.0f, top, 492.0f, bottom);
    return D2D1::RectF();
}

float PlaylistView::GetTotalContentHeight() const
{
    return m_filteredIndices.size() * kSlotHeight;
}

float PlaylistView::GetMaxScroll() const
{
    D2D1_RECT_F rcList = GetListRect();
    float viewH = rcList.bottom - rcList.top;
    return std::max(0.0f, GetTotalContentHeight() - viewH);
}

D2D1_RECT_F PlaylistView::GetScrollTrackRect() const
{
    D2D1_RECT_F rcList = GetListRect();
    return D2D1::RectF(rcList.right - kScrollbarWidth, rcList.top + 4.0f, rcList.right, rcList.bottom - 4.0f);
}

D2D1_RECT_F PlaylistView::GetScrollThumbRect() const
{
    float maxScroll = GetMaxScroll();
    if (maxScroll <= 0.0f) return D2D1::RectF();

    D2D1_RECT_F rcTrack = GetScrollTrackRect();
    float trackH = rcTrack.bottom - rcTrack.top;
    D2D1_RECT_F rcList = GetListRect();
    float viewH = rcList.bottom - rcList.top;
    float contentH = GetTotalContentHeight();

    float thumbH = std::clamp(trackH * (viewH / contentH), 28.0f, trackH);
    float ratio = m_scrollOffset / maxScroll;
    float top = rcTrack.top + (trackH - thumbH) * ratio;
    return D2D1::RectF(rcTrack.left, top, rcTrack.right, top + thumbH);
}

int PlaylistView::HitTestTitleBarButton(float x, float y) const
{
    for (int i = 0; i < 3; ++i)
    {
        D2D1_RECT_F rc = GetTitleBarButtonRect(i);
        if (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom)
        {
            return i;
        }
    }
    return -1;
}

int PlaylistView::HitTestVisibleCard(float x, float y, int* outInlineAction) const
{
    if (outInlineAction) *outInlineAction = 0;

    D2D1_RECT_F rcList = GetListRect();
    if (x < rcList.left || x > rcList.right || y < rcList.top || y > rcList.bottom)
    {
        return -1;
    }

    for (int v = 0; v < static_cast<int>(m_filteredIndices.size()); ++v)
    {
        D2D1_RECT_F cardRc = GetCardRect(v);
        if (x >= cardRc.left && x <= cardRc.right && y >= cardRc.top && y <= cardRc.bottom)
        {
            int itemIdx = m_filteredIndices[v];
            if (itemIdx != m_currentIndex && outInlineAction)
            {
                D2D1_RECT_F playRc = GetCardPlayButtonRect(cardRc);
                D2D1_RECT_F delRc = GetCardDeleteButtonRect(cardRc);
                if (x >= playRc.left && x <= playRc.right && y >= playRc.top && y <= playRc.bottom)
                {
                    *outInlineAction = 1;
                }
                else if (x >= delRc.left && x <= delRc.right && y >= delRc.top && y <= delRc.bottom)
                {
                    *outInlineAction = 2;
                }
            }
            return v;
        }
    }
    return -1;
}

int PlaylistView::HitTestFooterButton(float x, float y) const
{
    for (int i = 0; i < 3; ++i)
    {
        D2D1_RECT_F rc = GetFooterButtonRect(i);
        if (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom)
        {
            return i;
        }
    }
    return -1;
}

void PlaylistView::Draw()
{
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    if (FAILED(CreateDeviceResources())) return;

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0x161922, 1.0f));

    D2D1_SIZE_F rtSize = m_renderTarget->GetSize();

    // 1. Custom Title Bar
    {
        // Play badge icon on top-left
        D2D1_ROUNDED_RECT iconBadge = D2D1::RoundedRect(D2D1::RectF(18.0f, 10.0f, 36.0f, 28.0f), 4.0f, 4.0f);
        m_renderTarget->FillRoundedRectangle(iconBadge, m_badgeBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(iconBadge, m_btnBorderBrush.Get(), 0.8f);
        m_renderTarget->DrawTextW(L"▶", 1, m_subFormat.Get(), D2D1::RectF(19.0f, 10.0f, 36.0f, 28.0f), m_textBrush.Get());

        m_renderTarget->DrawTextW(
            L"NextVideoPlayer",
            15,
            m_captionFormat.Get(),
            D2D1::RectF(44.0f, 6.0f, 220.0f, 32.0f),
            m_subTextBrush.Get());

        // Min / Max / Close buttons
        for (int i = 0; i < 3; ++i)
        {
            D2D1_RECT_F rcBtn = GetTitleBarButtonRect(i);
            bool hover = (m_hoveredTitleBtn == i);
            if (hover)
            {
                m_renderTarget->FillRectangle(rcBtn, (i == 2) ? m_closeHoverBrush.Get() : m_btnHoverBrush.Get());
            }

            float cx = (rcBtn.left + rcBtn.right) * 0.5f;
            float cy = (rcBtn.top + rcBtn.bottom) * 0.5f;
            ID2D1SolidColorBrush* glyphBrush = hover ? m_textBrush.Get() : m_subTextBrush.Get();

            if (i == 0) // Minimize —
            {
                m_renderTarget->DrawLine(D2D1::Point2F(cx - 5.0f, cy), D2D1::Point2F(cx + 5.0f, cy), glyphBrush, 1.3f);
            }
            else if (i == 1) // Maximize □
            {
                m_renderTarget->DrawRectangle(D2D1::RectF(cx - 4.5f, cy - 4.5f, cx + 4.5f, cy + 4.5f), glyphBrush, 1.3f);
            }
            else if (i == 2) // Close ✕
            {
                m_renderTarget->DrawLine(D2D1::Point2F(cx - 4.5f, cy - 4.5f), D2D1::Point2F(cx + 4.5f, cy + 4.5f), glyphBrush, 1.5f);
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 4.5f, cy - 4.5f), D2D1::Point2F(cx - 4.5f, cy + 4.5f), glyphBrush, 1.5f);
            }
        }
    }

    // 2. Sub-header & Search Box
    {
        m_renderTarget->DrawTextW(
            L"재생목록 (Playlist)",
            15,
            m_titleFormat.Get(),
            D2D1::RectF(24.0f, 46.0f, 224.0f, 84.0f),
            m_textBrush.Get());

        // Count pill badge
        wchar_t countBuf[32]{};
        swprintf_s(countBuf, L"%zu개 항목", m_items.size());
        D2D1_RECT_F rcCount = D2D1::RectF(222.0f, 53.0f, 288.0f, 77.0f);
        D2D1_ROUNDED_RECT pillCount = D2D1::RoundedRect(rcCount, 12.0f, 12.0f);
        m_renderTarget->FillRoundedRectangle(pillCount, m_badgeBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(pillCount, m_btnBorderBrush.Get(), 0.8f);
        m_renderTarget->DrawTextW(countBuf, static_cast<UINT32>(wcslen(countBuf)), m_badgeFormat.Get(), rcCount, m_subTextBrush.Get());

        // Search box pill
        D2D1_RECT_F rcSearch = GetSearchBoxRect();
        D2D1_ROUNDED_RECT pillSearch = D2D1::RoundedRect(rcSearch, 17.0f, 17.0f);
        m_renderTarget->FillRoundedRectangle(pillSearch, m_btnBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(pillSearch, m_searchFocused ? m_cyanBrush.Get() : m_btnBorderBrush.Get(), 1.0f);

        // Magnifying glass vector icon
        float sx = rcSearch.left + 17.0f;
        float sy = (rcSearch.top + rcSearch.bottom) * 0.5f - 1.0f;
        m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(sx, sy), 5.0f, 5.0f), m_subTextBrush.Get(), 1.4f);
        m_renderTarget->DrawLine(D2D1::Point2F(sx + 3.6f, sy + 3.6f), D2D1::Point2F(sx + 7.5f, sy + 7.5f), m_subTextBrush.Get(), 1.5f);

        std::wstring displaySearch = m_searchQuery.empty() ? L"목록 검색..." : m_searchQuery;
        m_renderTarget->DrawTextW(
            displaySearch.c_str(),
            static_cast<UINT32>(displaySearch.length()),
            m_searchFormat.Get(),
            D2D1::RectF(rcSearch.left + 32.0f, rcSearch.top, rcSearch.right - 12.0f, rcSearch.bottom),
            m_searchQuery.empty() ? m_subTextBrush.Get() : m_textBrush.Get());
    }

    // 3. Video Item Cards
    D2D1_RECT_F rcList = GetListRect();
    bool hasScrollbar = (GetMaxScroll() > 0.0f);

    m_renderTarget->PushAxisAlignedClip(
        D2D1::RectF(rcList.left - 4.0f, rcList.top - 2.0f, rcList.right + 4.0f, rcList.bottom + 2.0f),
        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (m_filteredIndices.empty())
    {
        const wchar_t* emptyMsg = m_items.empty()
            ? L"재생목록이 비어 있습니다. 아래 [+ 파일 추가] 버튼을 눌러 영상을 추가하세요."
            : L"검색어와 일치하는 동영상이 없습니다.";
        m_renderTarget->DrawTextW(
            emptyMsg,
            static_cast<UINT32>(wcslen(emptyMsg)),
            m_btnFormat.Get(),
            D2D1::RectF(rcList.left, rcList.top + 80.0f, rcList.right, rcList.top + 140.0f),
            m_subTextBrush.Get());
    }
    else
    {
        for (int v = 0; v < static_cast<int>(m_filteredIndices.size()); ++v)
        {
            D2D1_RECT_F cardRc = GetCardRect(v);
            if (cardRc.bottom < rcList.top) continue;
            if (cardRc.top > rcList.bottom) break;

            int itemIdx = m_filteredIndices[v];
            bool isCurrent = (itemIdx == m_currentIndex);
            bool isSelected = (std::find(m_selectedIndices.begin(), m_selectedIndices.end(), itemIdx) != m_selectedIndices.end());
            bool isHovered = (v == m_hoveredVisibleIdx);

            D2D1_ROUNDED_RECT roundedCard = D2D1::RoundedRect(cardRc, 10.0f, 10.0f);

            if (isCurrent)
            {
                // Outer neon cyan glow + blue-slate card fill (just like Image 1!)
                D2D1_ROUNDED_RECT glowCard = D2D1::RoundedRect(
                    D2D1::RectF(cardRc.left - 1.5f, cardRc.top - 1.5f, cardRc.right + 1.5f, cardRc.bottom + 1.5f),
                    11.5f, 11.5f);
                m_renderTarget->DrawRoundedRectangle(glowCard, m_cyanGlowBrush.Get(), 3.5f);
                m_renderTarget->FillRoundedRectangle(roundedCard, m_cardActiveBgBrush.Get());
                m_renderTarget->DrawRoundedRectangle(roundedCard, m_cyanBrush.Get(), 1.6f);
            }
            else if (isSelected)
            {
                m_renderTarget->FillRoundedRectangle(roundedCard, m_cardHoverBrush.Get());
                m_renderTarget->DrawRoundedRectangle(roundedCard, m_cyanBrush.Get(), 1.2f);
            }
            else if (isHovered)
            {
                m_renderTarget->FillRoundedRectangle(roundedCard, m_cardHoverBrush.Get());
                m_renderTarget->DrawRoundedRectangle(roundedCard, m_btnBorderBrush.Get(), 1.0f);
            }
            else
            {
                m_renderTarget->FillRoundedRectangle(roundedCard, m_cardBgBrush.Get());
                m_renderTarget->DrawRoundedRectangle(roundedCard, m_cardBorderBrush.Get(), 1.0f);
            }

            // Left Video Thumbnail Box (w=96, h=54)
            D2D1_RECT_F thumbRc = D2D1::RectF(cardRc.left + 10.0f, cardRc.top + 9.0f, cardRc.left + 106.0f, cardRc.bottom - 9.0f);
            D2D1_ROUNDED_RECT thumbRounded = D2D1::RoundedRect(thumbRc, 6.0f, 6.0f);
            m_renderTarget->FillRoundedRectangle(thumbRounded, m_btnPressedBrush.Get());

            ID2D1Bitmap* thumbBmp = GetOrCreateThumbnail(m_items[itemIdx]);
            if (thumbBmp)
            {
                m_renderTarget->PushAxisAlignedClip(thumbRc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                m_renderTarget->DrawBitmap(thumbBmp, thumbRc, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                m_renderTarget->PopAxisAlignedClip();
            }
            else
            {
                // Sleek fallback thumbnail graphic
                float tcx = (thumbRc.left + thumbRc.right) * 0.5f;
                float tcy = (thumbRc.top + thumbRc.bottom) * 0.5f;
                m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(tcx, tcy), 11.0f, 11.0f), m_btnBorderBrush.Get(), 1.2f);
                m_renderTarget->DrawTextW(L"▶", 1, m_subFormat.Get(), thumbRc, m_cyanBrush.Get());
            }
            m_renderTarget->DrawRoundedRectangle(thumbRounded, m_cardBorderBrush.Get(), 0.8f);

            // Title & Metadata
            std::filesystem::path fsPath(m_items[itemIdx]);
            std::wstring filename = fsPath.filename().wstring();
            wchar_t titleBuf[512]{};
            swprintf_s(titleBuf, L"%02d. %s", itemIdx + 1, filename.c_str());

            float textLeft = thumbRc.right + 14.0f;
            float textRight = isCurrent ? (cardRc.right - 108.0f) : (cardRc.right - 96.0f);

            m_renderTarget->DrawTextW(
                titleBuf,
                static_cast<UINT32>(wcslen(titleBuf)),
                m_itemFormat.Get(),
                D2D1::RectF(textLeft, cardRc.top + 10.0f, textRight, cardRc.top + 42.0f),
                m_textBrush.Get());

            // File size / Extension badge
            std::error_code ec;
            auto fsize = std::filesystem::file_size(fsPath, ec);
            wchar_t metaBuf[32]{};
            if (!ec && fsize > 0)
            {
                double mb = static_cast<double>(fsize) / (1024.0 * 1024.0);
                if (mb >= 1000.0) swprintf_s(metaBuf, L"%.1f GB", mb / 1024.0);
                else swprintf_s(metaBuf, L"%.1f MB", mb);
            }
            else
            {
                std::wstring ext = fsPath.extension().wstring();
                if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towupper);
                swprintf_s(metaBuf, L"%s", ext.empty() ? L"VIDEO" : ext.c_str());
            }

            if (isCurrent)
            {
                // Equalizer bars + active blue badge on the right (just like Image 1!)
                float eqX = cardRc.right - 96.0f;
                float eqCy = (cardRc.top + cardRc.bottom) * 0.5f;
                const float barHeights[5] = { 7.0f, 15.0f, 19.0f, 12.0f, 8.0f };
                for (int b = 0; b < 5; ++b)
                {
                    float bx = eqX + b * 4.2f;
                    float bh = barHeights[b] * 0.5f;
                    D2D1_ROUNDED_RECT bar = D2D1::RoundedRect(
                        D2D1::RectF(bx, eqCy - bh, bx + 2.4f, eqCy + bh), 1.2f, 1.2f);
                    m_renderTarget->FillRoundedRectangle(bar, (b == 1) ? m_amberBrush.Get() : m_cyanBrush.Get());
                }

                D2D1_RECT_F badgeRc = D2D1::RectF(cardRc.right - 66.0f, eqCy - 11.0f, cardRc.right - 12.0f, eqCy + 11.0f);
                m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(badgeRc, 5.0f, 5.0f), m_activeBadgeBgBrush.Get());
                m_renderTarget->DrawTextW(
                    metaBuf,
                    static_cast<UINT32>(wcslen(metaBuf)),
                    m_subFormat.Get(),
                    badgeRc,
                    m_textBrush.Get());
            }
            else
            {
                // Sub-badge under title
                D2D1_RECT_F subBadgeRc = D2D1::RectF(textLeft, cardRc.top + 43.0f, textLeft + 52.0f, cardRc.top + 61.0f);
                m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(subBadgeRc, 4.0f, 4.0f), m_badgeBgBrush.Get());
                m_renderTarget->DrawTextW(
                    metaBuf,
                    static_cast<UINT32>(wcslen(metaBuf)),
                    m_subFormat.Get(),
                    subBadgeRc,
                    m_subTextBrush.Get());

                // Right inline Play [▶] & Delete [🗑] icon buttons (just like Card 02 in Image 1!)
                D2D1_RECT_F playRc = GetCardPlayButtonRect(cardRc);
                D2D1_RECT_F delRc = GetCardDeleteButtonRect(cardRc);

                bool hoverPlay = (isHovered && m_hoveredInlineAction == 1);
                bool hoverDel = (isHovered && m_hoveredInlineAction == 2);

                m_renderTarget->FillRoundedRectangle(
                    D2D1::RoundedRect(playRc, 6.0f, 6.0f),
                    hoverPlay ? m_btnHoverBrush.Get() : m_badgeBgBrush.Get());
                m_renderTarget->DrawRoundedRectangle(
                    D2D1::RoundedRect(playRc, 6.0f, 6.0f),
                    hoverPlay ? m_cyanBrush.Get() : m_btnBorderBrush.Get(), 1.0f);

                float pcx = (playRc.left + playRc.right) * 0.5f;
                float pcy = (playRc.top + playRc.bottom) * 0.5f;
                m_renderTarget->DrawEllipse(
                    D2D1::Ellipse(D2D1::Point2F(pcx, pcy), 7.5f, 7.5f),
                    hoverPlay ? m_cyanBrush.Get() : m_textBrush.Get(), 1.3f);
                m_renderTarget->DrawTextW(
                    L"▶", 1, m_subFormat.Get(),
                    D2D1::RectF(playRc.left + 1.0f, playRc.top, playRc.right, playRc.bottom),
                    hoverPlay ? m_cyanBrush.Get() : m_textBrush.Get());

                m_renderTarget->FillRoundedRectangle(
                    D2D1::RoundedRect(delRc, 6.0f, 6.0f),
                    hoverDel ? m_closeHoverBrush.Get() : m_badgeBgBrush.Get());
                m_renderTarget->DrawRoundedRectangle(
                    D2D1::RoundedRect(delRc, 6.0f, 6.0f),
                    hoverDel ? m_closeHoverBrush.Get() : m_btnBorderBrush.Get(), 1.0f);

                // Vector trash can icon
                float dcx = (delRc.left + delRc.right) * 0.5f;
                float dcy = (delRc.top + delRc.bottom) * 0.5f;
                ID2D1SolidColorBrush* trashBrush = m_textBrush.Get();
                m_renderTarget->DrawLine(D2D1::Point2F(dcx - 6.0f, dcy - 4.5f), D2D1::Point2F(dcx + 6.0f, dcy - 4.5f), trashBrush, 1.3f);
                m_renderTarget->DrawLine(D2D1::Point2F(dcx - 2.5f, dcy - 6.5f), D2D1::Point2F(dcx + 2.5f, dcy - 6.5f), trashBrush, 1.3f);
                m_renderTarget->DrawRectangle(D2D1::RectF(dcx - 4.5f, dcy - 4.0f, dcx + 4.5f, dcy + 6.0f), trashBrush, 1.2f);
                m_renderTarget->DrawLine(D2D1::Point2F(dcx - 1.5f, dcy - 1.5f), D2D1::Point2F(dcx - 1.5f, dcy + 3.5f), trashBrush, 1.1f);
                m_renderTarget->DrawLine(D2D1::Point2F(dcx + 1.5f, dcy - 1.5f), D2D1::Point2F(dcx + 1.5f, dcy + 3.5f), trashBrush, 1.1f);
            }
        }
    }

    m_renderTarget->PopAxisAlignedClip();

    // 4. Sleek Thin Scrollbar
    if (hasScrollbar)
    {
        D2D1_ROUNDED_RECT rcTrack = D2D1::RoundedRect(GetScrollTrackRect(), 2.0f, 2.0f);
        m_renderTarget->FillRoundedRectangle(rcTrack, m_scrollTrackBrush.Get());

        D2D1_ROUNDED_RECT rcThumb = D2D1::RoundedRect(GetScrollThumbRect(), 2.0f, 2.0f);
        m_renderTarget->FillRoundedRectangle(rcThumb, m_scrollThumbBrush.Get());
    }

    // 5. Footer Pill Buttons (just like Image 1!)
    m_renderTarget->DrawLine(
        D2D1::Point2F(0.0f, rtSize.height - kFooterHeight),
        D2D1::Point2F(rtSize.width, rtSize.height - kFooterHeight),
        m_separatorBrush.Get(),
        1.0f);

    const wchar_t* footerLabels[3] = {
        L"+  파일 추가 (Add)",
        L"▷  선택 재생 (Play)",
        L"🗑  비우기 (Clear)"
    };

    for (int i = 0; i < 3; ++i)
    {
        D2D1_RECT_F rcBtn = GetFooterButtonRect(i);
        D2D1_ROUNDED_RECT pillBtn = D2D1::RoundedRect(rcBtn, 19.0f, 19.0f);
        bool hover = (m_hoveredFooterBtn == i);
        bool press = (m_pressedFooterBtn == i);

        if (i == 0) // Glowing Cyan-Blue Pill Button
        {
            D2D1_ROUNDED_RECT glowPill = D2D1::RoundedRect(
                D2D1::RectF(rcBtn.left - 2.0f, rcBtn.top - 2.0f, rcBtn.right + 2.0f, rcBtn.bottom + 2.0f),
                21.0f, 21.0f);
            m_renderTarget->DrawRoundedRectangle(glowPill, m_cyanGlowBrush.Get(), 3.5f);

            if (m_addBtnGradBrush)
            {
                m_renderTarget->FillRoundedRectangle(pillBtn, m_addBtnGradBrush.Get());
            }
            else
            {
                m_renderTarget->FillRoundedRectangle(pillBtn, m_cyanBrush.Get());
            }
            m_renderTarget->DrawTextW(
                footerLabels[i],
                static_cast<UINT32>(wcslen(footerLabels[i])),
                m_btnBoldFormat.Get(),
                rcBtn,
                m_darkTextBrush.Get());
        }
        else // Dark Slate Pill Buttons
        {
            m_renderTarget->FillRoundedRectangle(
                pillBtn,
                press ? m_btnPressedBrush.Get() : (hover ? m_btnHoverBrush.Get() : m_btnBgBrush.Get()));
            m_renderTarget->DrawRoundedRectangle(
                pillBtn,
                hover ? m_cyanBrush.Get() : m_btnBorderBrush.Get(),
                1.0f);
            m_renderTarget->DrawTextW(
                footerLabels[i],
                static_cast<UINT32>(wcslen(footerLabels[i])),
                m_btnFormat.Get(),
                rcBtn,
                hover ? m_textBrush.Get() : m_subTextBrush.Get());
        }
    }

    // Outer 1px window border
    m_renderTarget->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, rtSize.width - 0.5f, rtSize.height - 0.5f), 8.0f, 8.0f),
        m_windowBorderBrush.Get(),
        1.2f);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

LRESULT CALLBACK PlaylistView::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* view = reinterpret_cast<PlaylistView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        view = static_cast<PlaylistView*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        view->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    return view ? view->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT PlaylistView::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) return 0; // Frameless client area fills entire window
        break;

    case WM_NCHITTEST:
    {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int border = 6;

        if (!IsZoomed(hwnd))
        {
            bool left = pt.x < border;
            bool right = pt.x >= rc.right - border;
            bool top = pt.y < border;
            bool bottom = pt.y >= rc.bottom - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
        }
        return HTCLIENT;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        Draw();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        if (m_renderTarget && width > 0 && height > 0)
        {
            m_renderTarget->Resize(D2D1::SizeU(width, height));
            ClampScroll();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 500;
        mmi->ptMinTrackSize.y = 380;
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        if (!m_trackingMouse)
        {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            m_trackingMouse = true;
        }

        if (m_isDraggingScroll)
        {
            float maxScroll = GetMaxScroll();
            D2D1_RECT_F rcTrack = GetScrollTrackRect();
            float trackH = rcTrack.bottom - rcTrack.top;
            D2D1_RECT_F rcThumb = GetScrollThumbRect();
            float thumbH = rcThumb.bottom - rcThumb.top;
            float usableTrack = trackH - thumbH;

            if (usableTrack > 0.0f)
            {
                float deltaY = y - m_dragStartY;
                m_scrollOffset = m_dragStartScrollOffset + (deltaY / usableTrack) * maxScroll;
                ClampScroll();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        int newTitleBtn = HitTestTitleBarButton(x, y);
        int newInlineAction = 0;
        int newHoverCard = HitTestVisibleCard(x, y, &newInlineAction);
        int newFooterBtn = HitTestFooterButton(x, y);

        if (newTitleBtn != m_hoveredTitleBtn ||
            newHoverCard != m_hoveredVisibleIdx ||
            newInlineAction != m_hoveredInlineAction ||
            newFooterBtn != m_hoveredFooterBtn)
        {
            m_hoveredTitleBtn = newTitleBtn;
            m_hoveredVisibleIdx = newHoverCard;
            m_hoveredInlineAction = newInlineAction;
            m_hoveredFooterBtn = newFooterBtn;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        m_trackingMouse = false;
        m_hoveredTitleBtn = -1;
        m_hoveredVisibleIdx = -1;
        m_hoveredInlineAction = 0;
        m_hoveredFooterBtn = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        SetFocus(hwnd);
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        int titleBtn = HitTestTitleBarButton(x, y);
        if (titleBtn >= 0)
        {
            m_pressedTitleBtn = titleBtn;
            SetCapture(hwnd);
            return 0;
        }

        // Drag window by custom title bar
        if (y < kTitleBarHeight)
        {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }

        // Check Search box click
        D2D1_RECT_F rcSearch = GetSearchBoxRect();
        bool clickedSearch = (x >= rcSearch.left && x <= rcSearch.right && y >= rcSearch.top && y <= rcSearch.bottom);
        if (clickedSearch != m_searchFocused)
        {
            m_searchFocused = clickedSearch;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (clickedSearch) return 0;

        int footerBtn = HitTestFooterButton(x, y);
        if (footerBtn >= 0)
        {
            m_pressedFooterBtn = footerBtn;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        D2D1_RECT_F rcThumb = GetScrollThumbRect();
        if (x >= rcThumb.left - 4.0f && x <= rcThumb.right + 4.0f && y >= rcThumb.top && y <= rcThumb.bottom)
        {
            m_isDraggingScroll = true;
            m_dragStartY = y;
            m_dragStartScrollOffset = m_scrollOffset;
            SetCapture(hwnd);
            return 0;
        }

        int inlineAction = 0;
        int visibleIdx = HitTestVisibleCard(x, y, &inlineAction);
        if (visibleIdx >= 0 && visibleIdx < static_cast<int>(m_filteredIndices.size()))
        {
            int itemIdx = m_filteredIndices[visibleIdx];
            if (inlineAction == 1) // Inline Play button clicked
            {
                m_selectedIndices = { itemIdx };
                if (m_onPlay) m_onPlay({ itemIdx });
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (inlineAction == 2) // Inline Delete button clicked
            {
                if (m_onRemove) m_onRemove({ itemIdx });
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrlHeld)
            {
                auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(), itemIdx);
                if (it != m_selectedIndices.end()) m_selectedIndices.erase(it);
                else m_selectedIndices.push_back(itemIdx);
            }
            else
            {
                m_selectedIndices = { itemIdx };
            }
            m_anchorIndex = itemIdx;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        int visibleIdx = HitTestVisibleCard(x, y, nullptr);
        if (visibleIdx >= 0 && visibleIdx < static_cast<int>(m_filteredIndices.size()))
        {
            int itemIdx = m_filteredIndices[visibleIdx];
            m_selectedIndices = { itemIdx };
            if (m_onPlay) m_onPlay({ itemIdx });
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        if (m_isDraggingScroll)
        {
            m_isDraggingScroll = false;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (m_pressedTitleBtn >= 0)
        {
            int btn = m_pressedTitleBtn;
            m_pressedTitleBtn = -1;
            ReleaseCapture();
            if (HitTestTitleBarButton(x, y) == btn)
            {
                if (btn == 0) ShowWindow(hwnd, SW_MINIMIZE);
                else if (btn == 1) ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                else if (btn == 2) SetVisible(false);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (m_pressedFooterBtn >= 0)
        {
            int btn = m_pressedFooterBtn;
            m_pressedFooterBtn = -1;
            ReleaseCapture();
            if (HitTestFooterButton(x, y) == btn)
            {
                if (btn == 0) { if (m_onAdd) m_onAdd(); }
                else if (btn == 1) { if (m_onPlay) m_onPlay(m_selectedIndices); }
                else if (btn == 2) { if (m_onClear) m_onClear(); }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        m_scrollOffset -= static_cast<float>(delta / WHEEL_DELTA) * (kSlotHeight * 1.5f);
        ClampScroll();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CHAR:
    {
        if (m_searchFocused)
        {
            wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == VK_BACK)
            {
                if (!m_searchQuery.empty()) m_searchQuery.pop_back();
            }
            else if (ch >= 32)
            {
                m_searchQuery.push_back(ch);
            }
            RebuildFilteredItems();
            m_scrollOffset = 0.0f;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (m_searchFocused && wParam == VK_ESCAPE)
        {
            m_searchQuery.clear();
            m_searchFocused = false;
            RebuildFilteredItems();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wParam == VK_DELETE && !m_searchFocused)
        {
            if (m_onRemove && !m_selectedIndices.empty()) m_onRemove(m_selectedIndices);
            return 0;
        }
        if (wParam == VK_RETURN && !m_searchFocused)
        {
            if (m_onPlay) m_onPlay(m_selectedIndices);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        SetVisible(false);
        return 0;

    case WM_NCDESTROY:
        DiscardDeviceResources();
        m_hwnd = nullptr;
        m_items.clear();
        m_selectedIndices.clear();
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
