#include "Views/ControlBarView.h"
#include "Views/FontHelper.h"
#include "Models/PlaybackSpeed.h"
#include <dwmapi.h>
#include <imm.h>
#include <algorithm>
#include <cmath>
#include <cwchar>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "imm32.lib")

namespace
{
    constexpr wchar_t kControlBarClassName[] = L"NextVideoPlayer_ControlBarWnd";
    constexpr wchar_t kSpeedPopupClassName[] = L"NextVideoPlayer_SpeedPopupWnd";
    constexpr wchar_t kSeekTooltipClassName[] = L"NextVideoPlayer_SeekTooltipWnd";
    constexpr int kSpeedPopupWidth = 112;
    constexpr int kSpeedRowHeight = 28;
    constexpr int kSpeedPopupPadY = 6;
    constexpr int kSpeedPopupGap = 6;
    constexpr int kSpeedPopupHeight =
        static_cast<int>(PlaybackSpeed::kPresets.size()) * kSpeedRowHeight + kSpeedPopupPadY * 2;
    constexpr int kSeekTooltipWidth = 78;
    constexpr int kSeekTooltipHeight = 28;
    constexpr int kSeekTooltipGap = 6;
}

ControlBarView::~ControlBarView()
{
    if (m_seekTooltipHwnd && IsWindow(m_seekTooltipHwnd)) DestroyWindow(m_seekTooltipHwnd);
    if (m_speedPopupHwnd && IsWindow(m_speedPopupHwnd)) DestroyWindow(m_speedPopupHwnd);
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}

bool ControlBarView::Initialize(HINSTANCE hInstance, HWND parentHwnd, int x, int y, int width, int height)
{
    m_hInstance = hInstance;
    m_parentHwnd = parentHwnd;

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    RegisterWindowClass(hInstance);

    m_hWnd = CreateWindowExW(
        0,
        kControlBarClassName,
        nullptr,
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        x,
        y,
        width,
        height,
        parentHwnd,
        nullptr,
        hInstance,
        this);

    if (!m_hWnd)
    {
        return false;
    }

    ImmAssociateContext(m_hWnd, nullptr);
    m_isVisible = true;
    SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return true;
}

HWND ControlBarView::GetHwnd() const
{
    return m_hWnd;
}

void ControlBarView::SetBounds(int x, int y, int width, int height, bool visible)
{
    m_isVisible = visible;
    if (!visible)
    {
        CloseSpeedPopup(false);
        HideSeekTooltip();
    }
    if (!m_hWnd)
    {
        return;
    }

    SetWindowPos(
        m_hWnd,
        HWND_TOP,
        x,
        y,
        width,
        height,
        (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE);
}

void ControlBarView::SetVisible(bool visible)
{
    if (m_isVisible == visible)
    {
        return;
    }

    m_isVisible = visible;
    if (!visible)
    {
        CloseSpeedPopup(false);
        HideSeekTooltip();
    }
    if (!m_hWnd)
    {
        return;
    }

    if (!visible)
    {
        CancelDrag();
        m_isTrackingMouse = false;
        m_hoverMousePt = D2D1::Point2F(-1.0f, -1.0f);
        ShowWindow(m_hWnd, SW_HIDE);
    }
    else
    {
        SetWindowPos(
            m_hWnd,
            HWND_TOP,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        RedrawWindow(
            m_hWnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    }
}

bool ControlBarView::IsVisible() const
{
    return m_isVisible;
}

bool ControlBarView::IsDragging() const
{
    return m_isDraggingSeek || m_isDraggingVolume || IsSpeedPopupOpen();
}

void ControlBarView::Render(const ControlBarState& state)
{
    m_state = state;
    if (state.duration <= 0.0)
    {
        HideSeekTooltip();
    }
    if (m_isVisible && m_hWnd && IsWindow(m_hWnd))
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
    if (IsSpeedPopupOpen())
    {
        InvalidateRect(m_speedPopupHwnd, nullptr, FALSE);
    }
}

void ControlBarView::Invalidate()
{
    if (m_isVisible && m_hWnd && IsWindow(m_hWnd))
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void ControlBarView::SetOnPlayPauseRequested(std::function<void()> handler) { m_onPlayPauseRequested = std::move(handler); }
void ControlBarView::SetOnStopRequested(std::function<void()> handler) { m_onStopRequested = std::move(handler); }
void ControlBarView::SetOnPrevRequested(std::function<void()> handler) { m_onPrevRequested = std::move(handler); }
void ControlBarView::SetOnNextRequested(std::function<void()> handler) { m_onNextRequested = std::move(handler); }
void ControlBarView::SetOnSeekAbsolute(std::function<void(double)> handler) { m_onSeekAbsolute = std::move(handler); }
void ControlBarView::SetOnVolumeChanged(std::function<void(int)> handler) { m_onVolumeChanged = std::move(handler); }
void ControlBarView::SetOnMuteToggled(std::function<void()> handler) { m_onMuteToggled = std::move(handler); }
void ControlBarView::SetOnSpeedStep(std::function<void(int)> handler) { m_onSpeedStep = std::move(handler); }
void ControlBarView::SetOnSpeedReset(std::function<void()> handler) { m_onSpeedReset = std::move(handler); }
void ControlBarView::SetOnSpeedSelected(std::function<void(double)> handler) { m_onSpeedSelected = std::move(handler); }
void ControlBarView::SetOnRepeatToggled(std::function<void()> handler) { m_onRepeatToggled = std::move(handler); }
void ControlBarView::SetOnSubtitleToggle(std::function<void()> handler) { m_onSubtitleToggle = std::move(handler); }
void ControlBarView::SetOnPlaylistRequested(std::function<void()> handler) { m_onPlaylistRequested = std::move(handler); }
void ControlBarView::SetOnSettingsRequested(std::function<void()> handler) { m_onSettingsRequested = std::move(handler); }

ATOM ControlBarView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW popupWc{};
    popupWc.cbSize = sizeof(WNDCLASSEXW);
    popupWc.style = CS_HREDRAW | CS_VREDRAW;
    popupWc.lpfnWndProc = ControlBarView::SpeedPopupWndProc;
    popupWc.hInstance = hInstance;
    popupWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    popupWc.hbrBackground = nullptr;
    popupWc.lpszClassName = kSpeedPopupClassName;
    RegisterClassExW(&popupWc);

    WNDCLASSEXW tooltipWc{};
    tooltipWc.cbSize = sizeof(WNDCLASSEXW);
    tooltipWc.style = CS_HREDRAW | CS_VREDRAW;
    tooltipWc.lpfnWndProc = ControlBarView::SeekTooltipWndProc;
    tooltipWc.hInstance = hInstance;
    tooltipWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    tooltipWc.hbrBackground = nullptr;
    tooltipWc.lpszClassName = kSeekTooltipClassName;
    RegisterClassExW(&tooltipWc);

    WNDCLASSEXW barWc{};
    barWc.cbSize = sizeof(WNDCLASSEXW);
    barWc.style = CS_HREDRAW | CS_VREDRAW;
    barWc.lpfnWndProc = ControlBarView::WindowProc;
    barWc.hInstance = hInstance;
    barWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    barWc.hbrBackground = nullptr;
    barWc.lpszClassName = kControlBarClassName;
    return RegisterClassExW(&barWc);
}

HRESULT ControlBarView::CreateDeviceIndependentResources()
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    const wchar_t* korFontName = L"Malgun Gothic";
    FontHelper::CreateCustomFontCollection(m_dwriteFactory.Get(), m_customFontCollection, korFontName);
    IDWriteFontCollection* korCollection = m_customFontCollection.Get();

    // 1. Korean UI button text (NanumSquare Bold 12.5pt)
    hr = m_dwriteFactory->CreateTextFormat(
        korFontName,
        korCollection,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.5f,
        L"ko-kr",
        m_uiTextFormat.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }
    m_uiTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_uiTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 2. Windows 10/11 Built-in Icon Font (Segoe Fluent Icons with fallback to Segoe MDL2 Assets)
    Microsoft::WRL::ComPtr<IDWriteFontCollection> sysCollection;
    m_dwriteFactory->GetSystemFontCollection(&sysCollection);
    const wchar_t* iconFontName = L"Segoe MDL2 Assets";
    if (sysCollection)
    {
        UINT32 idx = 0;
        BOOL exists = FALSE;
        if (SUCCEEDED(sysCollection->FindFamilyName(L"Segoe Fluent Icons", &idx, &exists)) && exists)
        {
            iconFontName = L"Segoe Fluent Icons";
        }
    }

    hr = m_dwriteFactory->CreateTextFormat(
        iconFontName,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        15.0f,
        L"en-us",
        m_iconTextFormat.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }
    m_iconTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_iconTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 3. Timer text (Segoe UI SemiBold 12.5pt, left-aligned)
    hr = m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.5f,
        L"en-us",
        m_timerTextFormat.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }
    m_timerTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_timerTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 4. Speed text (Segoe UI SemiBold 12.0pt, centered)
    hr = m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-us",
        m_speedTextFormat.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }
    m_speedTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_speedTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    return S_OK;
}

HRESULT ControlBarView::CreateDeviceResources()
{
    if (m_renderTarget)
    {
        return S_OK;
    }

    if (!m_hWnd || !IsWindow(m_hWnd))
    {
        return E_FAIL;
    }

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        0.0f,
        0.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT);

    HRESULT hr = m_d2dFactory->CreateDCRenderTarget(&props, m_renderTarget.GetAddressOf());
    if (FAILED(hr))
    {
        DiscardDeviceResources();
        return hr;
    }
    m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    // 100% Opaque 3-stop subtle dark gradient (은은한 다크 그라데이션)
    D2D1_GRADIENT_STOP stops[3]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x2E3038, 1.0f);
    stops[1].position = 0.35f;
    stops[1].color = D2D1::ColorF(0x1F2127, 1.0f);
    stops[2].position = 1.0f;
    stops[2].color = D2D1::ColorF(0x111216, 1.0f);

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
    if (SUCCEEDED(m_renderTarget->CreateGradientStopCollection(stops, 3, &stopCollection)))
    {
        m_renderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0f, 0.0f),
                D2D1::Point2F(0.0f, static_cast<float>(kDefaultHeight))),
            stopCollection.Get(),
            m_barGradientBrush.GetAddressOf());
    }

    if (FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF5F5F7, 1.0f), m_barTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xD4D4DC, 1.0f), m_barSubTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x18191D, 1.0f), m_barBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x262830, 1.0f), m_btnBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x353844, 1.0f), m_btnHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x484B58, 1.0f), m_btnBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2478E8, 1.0f), m_accentBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x3B8EFF, 1.0f), m_accentHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x383A44, 1.0f), m_trackBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x585B6B, 1.0f), m_trackHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFBE26, 1.0f), m_abMarkerBrush.GetAddressOf())))
    {
        DiscardDeviceResources();
        return hr;
    }
    return S_OK;
}

void ControlBarView::DiscardDeviceResources()
{
    m_abMarkerBrush.Reset();
    m_trackHoverBrush.Reset();
    m_trackBrush.Reset();
    m_accentHoverBrush.Reset();
    m_accentBrush.Reset();
    m_btnBorderBrush.Reset();
    m_btnHoverBrush.Reset();
    m_btnBrush.Reset();
    m_barBgBrush.Reset();
    m_barSubTextBrush.Reset();
    m_barTextBrush.Reset();
    m_barGradientBrush.Reset();
    m_renderTarget.Reset();
}

void ControlBarView::Draw(HDC hdc)
{
    if (!m_hWnd || !IsWindow(m_hWnd) || !hdc)
    {
        return;
    }

    if (FAILED(CreateDeviceResources()))
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    if (FAILED(m_renderTarget->BindDC(hdc, &rc)))
    {
        DiscardDeviceResources();
        return;
    }

    const float dpi = static_cast<float>(FontHelper::GetWindowDpi(m_hWnd));
    m_renderTarget->SetDpi(dpi, dpi);
    m_renderTarget->BeginDraw();

    D2D1_SIZE_F rtSize = m_renderTarget->GetSize();
    D2D1_RECT_F barRect = D2D1::RectF(0.0f, 0.0f, rtSize.width, rtSize.height);

    // 1. 100% Opaque Subtle Dark Gradient Background (은은한 다크 그라데이션)
    m_renderTarget->Clear(D2D1::ColorF(0x16171B, 1.0f));
    if (m_barGradientBrush)
    {
        m_barGradientBrush->SetStartPoint(D2D1::Point2F(0.0f, 0.0f));
        m_barGradientBrush->SetEndPoint(D2D1::Point2F(0.0f, rtSize.height));
        m_renderTarget->FillRectangle(barRect, m_barGradientBrush.Get());
    }
    else
    {
        m_barBgBrush->SetColor(D2D1::ColorF(0x1E1E1E, 1.0f));
        m_renderTarget->FillRectangle(barRect, m_barBgBrush.Get());
    }

    // Subtle 1px top highlight border separator
    m_renderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, rtSize.width, 1.0f), m_btnBorderBrush.Get());

    // 2. Thin, Sleek Seek Bar with Hover/Drag Highlight
    m_seekBarRect = D2D1::RectF(16.0f, 4.0f, std::max(32.0f, rtSize.width - 16.0f), 24.0f);
    bool hoverSeek = m_isDraggingSeek ||
        (m_isTrackingMouse && PtInRectF(m_seekBarRect, m_hoverMousePt.x, m_hoverMousePt.y));

    float trackCenterY = 14.0f;
    float trackHalfH = hoverSeek ? 3.0f : 1.5f;
    D2D1_ROUNDED_RECT trackRounded = D2D1::RoundedRect(
        D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, m_seekBarRect.right, trackCenterY + trackHalfH),
        trackHalfH,
        trackHalfH);
    m_renderTarget->FillRoundedRectangle(trackRounded, m_trackBrush.Get());

    float seekWidth = m_seekBarRect.right - m_seekBarRect.left;

    // Hover preview bar along seek track
    if (hoverSeek && m_hoverMousePt.x >= m_seekBarRect.left)
    {
        float previewX = std::clamp(m_hoverMousePt.x, m_seekBarRect.left, m_seekBarRect.right);
        D2D1_ROUNDED_RECT previewRounded = D2D1::RoundedRect(
            D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, previewX, trackCenterY + trackHalfH),
            trackHalfH,
            trackHalfH);
        m_renderTarget->FillRoundedRectangle(previewRounded, m_trackHoverBrush.Get());
    }

    float progressRatio = 0.0f;
    if (m_state.duration > 0.0)
    {
        progressRatio = static_cast<float>(std::clamp(m_state.position / m_state.duration, 0.0, 1.0));
    }

    float filledX = m_seekBarRect.left + seekWidth * progressRatio;
    if (filledX > m_seekBarRect.left)
    {
        D2D1_ROUNDED_RECT filledRounded = D2D1::RoundedRect(
            D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, filledX, trackCenterY + trackHalfH),
            trackHalfH,
            trackHalfH);
        m_renderTarget->FillRoundedRectangle(
            filledRounded,
            hoverSeek ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }

    // A-B markers
    if (m_state.duration > 0.0)
    {
        if (m_state.abLoopA >= 0.0)
        {
            float ax = m_seekBarRect.left + seekWidth * static_cast<float>(std::clamp(m_state.abLoopA / m_state.duration, 0.0, 1.0));
            m_renderTarget->FillRectangle(D2D1::RectF(ax - 1.5f, 7.0f, ax + 1.5f, 21.0f), m_abMarkerBrush.Get());
        }
        if (m_state.abLoopB >= 0.0)
        {
            float bx = m_seekBarRect.left + seekWidth * static_cast<float>(std::clamp(m_state.abLoopB / m_state.duration, 0.0, 1.0));
            m_renderTarget->FillRectangle(D2D1::RectF(bx - 1.5f, 7.0f, bx + 1.5f, 21.0f), m_abMarkerBrush.Get());
        }
    }

    // Seek thumb with hover highlight ring
    if (hoverSeek)
    {
        m_accentHoverBrush->SetOpacity(0.28f);
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(filledX, trackCenterY), 9.5f, 9.5f), m_accentHoverBrush.Get());
        m_accentHoverBrush->SetOpacity(1.0f);
    }
    float thumbRadius = hoverSeek ? 6.0f : 4.2f;
    m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(filledX, trackCenterY), thumbRadius, thumbRadius), m_barTextBrush.Get());

    // 3. Transport & Control Row (Windows 10/11 Fluent/MDL2 Vector Icons + Segoe UI Timer + NanumSquare Korean UI)
    float rowTop = 30.0f;
    float rowBottom = 64.0f;
    float curX = 14.0f;

    // Play / Pause: \uE768 (▶), \uE769 (⏸)
    bool isPlaying = (m_state.playbackState == PlaybackState::Playing);
    m_btnPlayPause = D2D1::RectF(curX, rowTop, curX + 38.0f, rowBottom);
    DrawButton(m_btnPlayPause, isPlaying ? L"\uE769" : L"\uE768", m_iconTextFormat.Get(), isPlaying);
    curX += 44.0f;

    // Stop: \uE71A (⏹)
    m_btnStop = D2D1::RectF(curX, rowTop, curX + 36.0f, rowBottom);
    DrawButton(m_btnStop, L"\uE71A", m_iconTextFormat.Get(), false);
    curX += 42.0f;

    // Prev: \uE892 (⏮)
    m_btnPrev = D2D1::RectF(curX, rowTop, curX + 36.0f, rowBottom);
    DrawButton(m_btnPrev, L"\uE892", m_iconTextFormat.Get(), false);
    curX += 42.0f;

    // Next: \uE893 (⏭)
    m_btnNext = D2D1::RectF(curX, rowTop, curX + 36.0f, rowBottom);
    DrawButton(m_btnNext, L"\uE893", m_iconTextFormat.Get(), false);
    curX += 46.0f;

    // Timer in Segoe UI: 00:00:00 / 00:00:00
    std::wstring posStr = FormatTime(m_state.position);
    std::wstring durStr = L" / " + FormatTime(m_state.duration);
    if (m_state.abLoopA >= 0.0 || m_state.abLoopB >= 0.0)
    {
        durStr += L"  [A-B]";
    }
    std::wstring fullTimeStr = posStr + durStr;

    D2D1_RECT_F timeRect = D2D1::RectF(curX, rowTop, curX + 220.0f, rowBottom);
    m_renderTarget->DrawTextW(
        fullTimeStr.c_str(),
        static_cast<UINT32>(fullTimeStr.length()),
        m_timerTextFormat.Get(),
        timeRect,
        m_barTextBrush.Get());

    // Right-side controls (laid out right-to-left)
    float rightX = rtSize.width - 14.0f;

    // Settings: \uE713 (⚙)
    m_btnSettings = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnSettings, L"\uE713", m_iconTextFormat.Get(), false);
    rightX -= 42.0f;

    // Playlist: \uE90B (📑)
    m_btnPlaylist = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnPlaylist, L"\uE90B", m_iconTextFormat.Get(), m_state.playlistOpen);
    rightX -= 42.0f;

    // Subtitle Toggle (Korean UI text in NanumSquare)
    m_btnSub = D2D1::RectF(rightX - 48.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnSub, L"자막", m_uiTextFormat.Get(), m_state.subtitleVisible);
    rightX -= 54.0f;

    // Repeat: \uE8EE (🔁) - Default OFF
    m_btnRepeat = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnRepeat, L"\uE8EE", m_iconTextFormat.Get(), m_state.repeatEnabled);
    rightX -= 46.0f;

    // Speed controls (+, 1.00x, -) in Segoe UI
    m_btnSpeedPlus = D2D1::RectF(rightX - 28.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedPlus, L"+", m_speedTextFormat.Get(), false);
    rightX -= 32.0f;

    WCHAR speedBuf[32]{};
    swprintf_s(speedBuf, L"%.2fx", m_state.speed);
    m_btnSpeedReset = D2D1::RectF(rightX - 60.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedReset, speedBuf, m_speedTextFormat.Get(), std::abs(m_state.speed - 1.0) > 0.04);
    rightX -= 64.0f;

    m_btnSpeedMinus = D2D1::RectF(rightX - 28.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedMinus, L"-", m_speedTextFormat.Get(), false);
    rightX -= 42.0f;

    // Volume slider with hover highlight
    float volCenterY = (rowTop + rowBottom) * 0.5f;
    m_volSliderRect = D2D1::RectF(rightX - 80.0f, rowTop + 6.0f, rightX, rowBottom - 6.0f);
    bool hoverVol = m_isDraggingVolume ||
        (m_isTrackingMouse && PtInRectF(m_volSliderRect, m_hoverMousePt.x, m_hoverMousePt.y));
    float volHalfH = hoverVol ? 2.5f : 1.5f;

    D2D1_ROUNDED_RECT volTrack = D2D1::RoundedRect(
        D2D1::RectF(m_volSliderRect.left, volCenterY - volHalfH, m_volSliderRect.right, volCenterY + volHalfH),
        volHalfH,
        volHalfH);
    m_renderTarget->FillRoundedRectangle(volTrack, hoverVol ? m_trackHoverBrush.Get() : m_trackBrush.Get());

    float volRatio = m_state.muted ? 0.0f : static_cast<float>(std::clamp(m_state.volume, 0, 100)) / 100.0f;
    float volFillX = m_volSliderRect.left + (m_volSliderRect.right - m_volSliderRect.left) * volRatio;
    if (volFillX > m_volSliderRect.left)
    {
        D2D1_ROUNDED_RECT volFill = D2D1::RoundedRect(
            D2D1::RectF(m_volSliderRect.left, volCenterY - volHalfH, volFillX, volCenterY + volHalfH),
            volHalfH,
            volHalfH);
        m_renderTarget->FillRoundedRectangle(volFill, hoverVol ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }

    float volThumbR = hoverVol ? 5.2f : 3.8f;
    m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(volFillX, volCenterY), volThumbR, volThumbR), m_barTextBrush.Get());
    rightX -= 88.0f;

    // Volume / Mute Icon: \uE767 (🔊), \uE74F (🔇)
    bool isMuted = m_state.muted || (m_state.volume <= 0);
    m_btnMute = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnMute, isMuted ? L"\uE74F" : L"\uE767", m_iconTextFormat.Get(), m_state.muted);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void ControlBarView::DrawButton(const D2D1_RECT_F& rect, const std::wstring& label, IDWriteTextFormat* textFormat, bool active)
{
    bool hovered = m_isTrackingMouse && PtInRectF(rect, m_hoverMousePt.x, m_hoverMousePt.y);
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 6.0f, 6.0f);

    if (active)
    {
        m_renderTarget->FillRoundedRectangle(rounded, hovered ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }
    else if (hovered)
    {
        m_renderTarget->FillRoundedRectangle(rounded, m_btnHoverBrush.Get());
        m_renderTarget->DrawRoundedRectangle(rounded, m_btnBorderBrush.Get(), 1.0f);
    }
    else
    {
        m_renderTarget->FillRoundedRectangle(rounded, m_btnBrush.Get());
    }

    IDWriteTextFormat* fmt = textFormat ? textFormat : m_uiTextFormat.Get();
    m_renderTarget->DrawTextW(
        label.c_str(),
        static_cast<UINT32>(label.length()),
        fmt,
        rect,
        (active || hovered) ? m_barTextBrush.Get() : m_barSubTextBrush.Get());
}

std::wstring ControlBarView::FormatTime(double seconds)
{
    if (seconds < 0.0 || std::isnan(seconds))
    {
        seconds = 0.0;
    }

    int totalSec = static_cast<int>(seconds);
    int hrs = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    int secs = totalSec % 60;

    WCHAR buf[32]{};
    swprintf_s(buf, L"%02d:%02d:%02d", hrs, mins, secs);
    return buf;
}

bool ControlBarView::PtInRectF(const D2D1_RECT_F& rect, float x, float y)
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

LRESULT CALLBACK ControlBarView::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ControlBarView* view = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<ControlBarView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_hWnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<ControlBarView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT ControlBarView::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        Draw(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        OnLButtonDown(FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi), FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi));
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        return 0;
    case WM_MOUSEMOVE:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        OnMouseMove(FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi), FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi));
        return 0;
    }
    case WM_MOUSELEAVE:
        OnMouseLeave();
        return 0;
    case WM_LBUTTONUP:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        OnLButtonUp(FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi), FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi));
        return 0;
    }
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        CancelDrag();
        return 0;
    case WM_MOUSEWHEEL:
        if (m_parentHwnd)
        {
            SendMessageW(m_parentHwnd, WM_MOUSEWHEEL, wParam, lParam);
        }
        return 0;
    case WM_KEYDOWN:
        if (m_parentHwnd)
        {
            SendMessageW(m_parentHwnd, WM_KEYDOWN, wParam, lParam);
        }
        return 0;
    case WM_DESTROY:
        HideSeekTooltip();
        if (m_seekTooltipHwnd && IsWindow(m_seekTooltipHwnd))
        {
            DestroyWindow(m_seekTooltipHwnd);
            m_seekTooltipHwnd = nullptr;
        }
        CloseSpeedPopup(false);
        if (m_speedPopupHwnd && IsWindow(m_speedPopupHwnd))
        {
            DestroyWindow(m_speedPopupHwnd);
            m_speedPopupHwnd = nullptr;
        }
        CancelDrag();
        DiscardDeviceResources();
        return 0;
    case WM_NCDESTROY:
        m_hWnd = nullptr;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void ControlBarView::OnLButtonDown(float x, float y)
{
    const bool wasSpeedPopupOpen = IsSpeedPopupOpen();
    const bool justClosedPopup = (m_speedPopupClosedTick != 0 && GetTickCount64() - m_speedPopupClosedTick < 150);
    CloseSpeedPopup(false);

    if (HWND rootHwnd = GetAncestor(m_hWnd, GA_ROOT))
    {
        SetFocus(rootHwnd);
    }
    else if (m_parentHwnd)
    {
        SetFocus(m_parentHwnd);
    }

    if (PtInRectF(m_seekBarRect, x, y))
    {
        m_isDraggingSeek = true;
        SetCapture(m_hWnd);
        SeekFromMouseX(x);
        UpdateSeekTooltip(x);
        return;
    }

    if (PtInRectF(m_volSliderRect, x, y))
    {
        m_isDraggingVolume = true;
        SetCapture(m_hWnd);
        VolumeFromMouseX(x);
        return;
    }

    if (PtInRectF(m_btnPlayPause, x, y) && m_onPlayPauseRequested) m_onPlayPauseRequested();
    else if (PtInRectF(m_btnStop, x, y) && m_onStopRequested) m_onStopRequested();
    else if (PtInRectF(m_btnPrev, x, y) && m_onPrevRequested) m_onPrevRequested();
    else if (PtInRectF(m_btnNext, x, y) && m_onNextRequested) m_onNextRequested();
    else if (PtInRectF(m_btnMute, x, y) && m_onMuteToggled) m_onMuteToggled();
    else if (PtInRectF(m_btnSpeedMinus, x, y) && m_onSpeedStep) m_onSpeedStep(-1);
    else if (PtInRectF(m_btnSpeedReset, x, y))
    {
        if (!wasSpeedPopupOpen && !justClosedPopup)
        {
            ShowSpeedPopup();
        }
    }
    else if (PtInRectF(m_btnSpeedPlus, x, y) && m_onSpeedStep) m_onSpeedStep(1);
    else if (PtInRectF(m_btnRepeat, x, y) && m_onRepeatToggled) m_onRepeatToggled();
    else if (PtInRectF(m_btnSub, x, y) && m_onSubtitleToggle) m_onSubtitleToggle();
    else if (PtInRectF(m_btnPlaylist, x, y) && m_onPlaylistRequested) m_onPlaylistRequested();
    else if (PtInRectF(m_btnSettings, x, y) && m_onSettingsRequested) m_onSettingsRequested();
}

void ControlBarView::OnMouseMove(float x, float y)
{
    m_hoverMousePt = D2D1::Point2F(x, y);

    if (!m_isTrackingMouse && m_hWnd)
    {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
        if (TrackMouseEvent(&tme))
        {
            m_isTrackingMouse = true;
        }
    }

    if (m_isDraggingSeek)
    {
        SeekFromMouseX(x);
    }
    else if (m_isDraggingVolume)
    {
        VolumeFromMouseX(x);
    }

    if (m_isDraggingSeek || PtInRectF(m_seekBarRect, x, y))
    {
        UpdateSeekTooltip(x);
    }
    else
    {
        HideSeekTooltip();
    }

    if (m_hWnd)
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void ControlBarView::OnMouseLeave()
{
    m_isTrackingMouse = false;
    m_hoverMousePt = D2D1::Point2F(-1.0f, -1.0f);
    if (!m_isDraggingSeek)
    {
        HideSeekTooltip();
    }
    if (m_hWnd)
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void ControlBarView::OnLButtonUp(float, float)
{
    CancelDrag();
}

void ControlBarView::CancelDrag()
{
    m_isDraggingSeek = false;
    m_isDraggingVolume = false;
    if (m_hWnd && GetCapture() == m_hWnd) ReleaseCapture();
    POINT cursorPt{};
    if (!m_hWnd || !GetCursorPos(&cursorPt) || !ScreenToClient(m_hWnd, &cursorPt) ||
        !PtInRectF(m_seekBarRect, static_cast<float>(cursorPt.x), static_cast<float>(cursorPt.y)))
    {
        HideSeekTooltip();
    }
    Invalidate();
}

double ControlBarView::SeekRatioFromMouseX(float x) const
{
    float width = std::max(1.0f, m_seekBarRect.right - m_seekBarRect.left);
    return std::clamp(static_cast<double>((x - m_seekBarRect.left) / width), 0.0, 1.0);
}

void ControlBarView::SeekFromMouseX(float x)
{
    if (m_state.duration <= 0.0 || !m_onSeekAbsolute)
    {
        return;
    }

    m_onSeekAbsolute(SeekRatioFromMouseX(x) * m_state.duration);
}

void ControlBarView::VolumeFromMouseX(float x)
{
    if (!m_onVolumeChanged)
    {
        return;
    }

    float width = std::max(1.0f, m_volSliderRect.right - m_volSliderRect.left);
    float ratio = std::clamp((x - m_volSliderRect.left) / width, 0.0f, 1.0f);
    int volume = static_cast<int>(std::round(ratio * 100.0f));
    m_onVolumeChanged(volume);
}

bool ControlBarView::IsSpeedPopupOpen() const
{
    return m_speedPopupHwnd && IsWindow(m_speedPopupHwnd) && IsWindowVisible(m_speedPopupHwnd);
}

void ControlBarView::ShowSpeedPopup()
{
    if (!m_hWnd || !IsWindow(m_hWnd))
    {
        return;
    }

    HWND ownerHwnd = GetRootOwnerHwnd();
    const UINT dpi = FontHelper::GetWindowDpi(m_hWnd ? m_hWnd : ownerHwnd);
    const int popupW = FontHelper::ScaleDipToPx(kSpeedPopupWidth, dpi);
    const int popupH = FontHelper::ScaleDipToPx(kSpeedPopupHeight, dpi);
    const int popupGap = FontHelper::ScaleDipToPx(kSpeedPopupGap, dpi);

    if (!m_speedPopupHwnd || !IsWindow(m_speedPopupHwnd))
    {
        m_speedPopupHwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            kSpeedPopupClassName,
            nullptr,
            WS_POPUP | WS_CLIPSIBLINGS,
            0, 0, popupW, popupH,
            ownerHwnd,
            nullptr,
            m_hInstance,
            this);
        if (!m_speedPopupHwnd)
        {
            return;
        }

        ImmAssociateContext(m_speedPopupHwnd, nullptr);
        DWORD cornerPref = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(m_speedPopupHwnd, 33, &cornerPref, sizeof(cornerPref));
    }

    POINT ptBtn{
        FontHelper::ScaleDipToPx(static_cast<int>(std::lround((m_btnSpeedReset.left + m_btnSpeedReset.right) * 0.5f)), dpi),
        FontHelper::ScaleDipToPx(static_cast<int>(std::lround(m_btnSpeedReset.top)), dpi)
    };
    ClientToScreen(m_hWnd, &ptBtn);

    int popupX = ptBtn.x - popupW / 2;
    int popupY = ptBtn.y - popupH - popupGap;

    HMONITOR hMonitor = MonitorFromWindow(ownerHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMonitor, &mi))
    {
        popupX = std::clamp(popupX, static_cast<int>(mi.rcMonitor.left),
            static_cast<int>(mi.rcMonitor.right) - popupW);
        if (popupY < mi.rcMonitor.top)
        {
            const int btnHeight = FontHelper::ScaleDipToPx(static_cast<int>(std::lround(m_btnSpeedReset.bottom - m_btnSpeedReset.top)), dpi);
            popupY = ptBtn.y + btnHeight + popupGap;
        }
        popupY = std::clamp(popupY, static_cast<int>(mi.rcMonitor.top),
            static_cast<int>(mi.rcMonitor.bottom) - popupH);
    }

    m_hoveredSpeedIndex = -1;
    m_isTrackingPopupMouse = false;

    const bool ownerTopMost = (GetWindowLongPtrW(ownerHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    SetWindowPos(
        m_speedPopupHwnd,
        ownerTopMost ? HWND_TOPMOST : HWND_TOP,
        popupX,
        popupY,
        popupW,
        popupH,
        SWP_SHOWWINDOW);
    SetFocus(m_speedPopupHwnd);
    InvalidateRect(m_speedPopupHwnd, nullptr, FALSE);
}

void ControlBarView::CloseSpeedPopup(bool restoreFocus)
{
    if (m_isClosingSpeedPopup || !IsSpeedPopupOpen())
    {
        return;
    }

    m_isClosingSpeedPopup = true;
    m_speedPopupClosedTick = GetTickCount64();
    m_hoveredSpeedIndex = -1;
    m_isTrackingPopupMouse = false;
    ShowWindow(m_speedPopupHwnd, SW_HIDE);

    if (restoreFocus)
    {
        if (HWND rootHwnd = GetRootOwnerHwnd(); rootHwnd && IsWindow(rootHwnd))
        {
            SetFocus(rootHwnd);
        }
    }
    m_isClosingSpeedPopup = false;
}

void ControlBarView::DrawSpeedPopup(HDC hdc)
{
    if (!m_speedPopupHwnd || !IsWindow(m_speedPopupHwnd) || !hdc)
    {
        return;
    }

    if (FAILED(CreateDeviceResources()))
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_speedPopupHwnd, &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    if (FAILED(m_renderTarget->BindDC(hdc, &rc)))
    {
        DiscardDeviceResources();
        return;
    }

    const float dpi = static_cast<float>(FontHelper::GetWindowDpi(m_speedPopupHwnd));
    m_renderTarget->SetDpi(dpi, dpi);
    m_renderTarget->BeginDraw();

    const D2D1_SIZE_F rtSize = m_renderTarget->GetSize();
    m_renderTarget->Clear(D2D1::ColorF(0x1B1E28, 1.0f));
    m_renderTarget->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, rtSize.width - 0.5f, rtSize.height - 0.5f), 8.0f, 8.0f),
        m_btnBorderBrush.Get(),
        1.0f);

    for (int i = 0; i < static_cast<int>(PlaybackSpeed::kPresets.size()); ++i)
    {
        const double preset = PlaybackSpeed::kPresets[i];
        const bool selected = std::abs(m_state.speed - preset) < 0.015;
        const bool hovered = (i == m_hoveredSpeedIndex);

        const float top = static_cast<float>(kSpeedPopupPadY + i * kSpeedRowHeight + 1);
        const float bottom = static_cast<float>(kSpeedPopupPadY + (i + 1) * kSpeedRowHeight - 1);
        D2D1_RECT_F itemRect = D2D1::RectF(6.0f, top, rtSize.width - 6.0f, bottom);
        D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(itemRect, 5.0f, 5.0f);

        if (selected)
        {
            m_renderTarget->FillRoundedRectangle(rounded, hovered ? m_accentHoverBrush.Get() : m_accentBrush.Get());
        }
        else if (hovered)
        {
            m_renderTarget->FillRoundedRectangle(rounded, m_btnHoverBrush.Get());
            m_renderTarget->DrawRoundedRectangle(rounded, m_btnBorderBrush.Get(), 1.0f);
        }

        WCHAR buf[32]{};
        swprintf_s(buf, L"%.2fx", preset);
        m_renderTarget->DrawTextW(
            buf,
            static_cast<UINT32>(wcslen(buf)),
            m_speedTextFormat.Get(),
            itemRect,
            (selected || hovered) ? m_barTextBrush.Get() : m_barSubTextBrush.Get());
    }

    if (m_renderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

LRESULT CALLBACK ControlBarView::SpeedPopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ControlBarView* view = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<ControlBarView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_speedPopupHwnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<ControlBarView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleSpeedPopupMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT ControlBarView::HandleSpeedPopupMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto hitTestRow = [hWnd](int px, int py) -> int {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        const float x = FontHelper::PxToDip(px, dpi);
        const float y = FontHelper::PxToDip(py, dpi);
        if (x < 6.0f || x > static_cast<float>(kSpeedPopupWidth - 6) || y < static_cast<float>(kSpeedPopupPadY))
        {
            return -1;
        }
        const int idx = static_cast<int>((y - static_cast<float>(kSpeedPopupPadY)) / static_cast<float>(kSpeedRowHeight));
        if (idx < 0 || idx >= static_cast<int>(PlaybackSpeed::kPresets.size()))
        {
            return -1;
        }
        return idx;
    };

    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawSpeedPopup(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
    {
        if (!m_isTrackingPopupMouse)
        {
            TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            m_isTrackingPopupMouse = TrackMouseEvent(&tme) != FALSE;
        }
        const int idx = hitTestRow(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (idx != m_hoveredSpeedIndex)
        {
            m_hoveredSpeedIndex = idx;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        m_isTrackingPopupMouse = false;
        if (m_hoveredSpeedIndex != -1)
        {
            m_hoveredSpeedIndex = -1;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
    {
        const int idx = hitTestRow(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (idx >= 0)
        {
            const double preset = PlaybackSpeed::kPresets[idx];
            CloseSpeedPopup(true);
            if (m_onSpeedSelected)
            {
                m_onSpeedSelected(preset);
            }
        }
        return 0;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_PROCESSKEY)
        {
            wParam = ImmGetVirtualKey(hWnd);
        }
        if (wParam == VK_ESCAPE)
        {
            CloseSpeedPopup(true);
            return 0;
        }
        HWND rootHwnd = m_hWnd ? GetAncestor(m_hWnd, GA_ROOT) : nullptr;
        CloseSpeedPopup(true);
        if (rootHwnd && IsWindow(rootHwnd))
        {
            SendMessageW(rootHwnd, WM_KEYDOWN, wParam, lParam);
        }
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            CloseSpeedPopup(false);
        }
        return 0;
    case WM_KILLFOCUS:
        CloseSpeedPopup(false);
        return 0;
    case WM_NCDESTROY:
        if (m_speedPopupHwnd == hWnd)
        {
            m_speedPopupHwnd = nullptr;
        }
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

HWND ControlBarView::GetRootOwnerHwnd() const
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        if (HWND rootHwnd = GetAncestor(m_hWnd, GA_ROOT))
        {
            return rootHwnd;
        }
    }
    return m_parentHwnd ? m_parentHwnd : m_hWnd;
}

void ControlBarView::UpdateSeekTooltip(float mouseX)
{
    if (!m_isVisible || !m_hWnd || !IsWindow(m_hWnd) ||
        m_state.duration <= 0.0 || m_seekBarRect.right <= m_seekBarRect.left)
    {
        HideSeekTooltip();
        return;
    }

    HWND ownerHwnd = GetRootOwnerHwnd();
    const UINT dpi = FontHelper::GetWindowDpi(m_hWnd ? m_hWnd : ownerHwnd);
    const int tipW = FontHelper::ScaleDipToPx(kSeekTooltipWidth, dpi);
    const int tipH = FontHelper::ScaleDipToPx(kSeekTooltipHeight, dpi);
    const int tipGap = FontHelper::ScaleDipToPx(kSeekTooltipGap, dpi);
    const int barH = FontHelper::ScaleDipToPx(kDefaultHeight, dpi);

    if (!m_seekTooltipHwnd || !IsWindow(m_seekTooltipHwnd))
    {
        m_seekTooltipHwnd = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            kSeekTooltipClassName,
            nullptr,
            WS_POPUP | WS_CLIPSIBLINGS,
            0, 0, tipW, tipH,
            ownerHwnd,
            nullptr,
            m_hInstance,
            this);
        if (!m_seekTooltipHwnd)
        {
            return;
        }

        ImmAssociateContext(m_seekTooltipHwnd, nullptr);
        DWORD cornerPref = 2; // DWMWCP_ROUND
        DwmSetWindowAttribute(m_seekTooltipHwnd, 33, &cornerPref, sizeof(cornerPref));
    }

    const float clampedX = std::clamp(mouseX, m_seekBarRect.left, m_seekBarRect.right);
    m_seekTooltipText = FormatTime(SeekRatioFromMouseX(clampedX) * m_state.duration);

    POINT pt{ FontHelper::ScaleDipToPx(static_cast<int>(std::lround(clampedX)), dpi), 0 };
    ClientToScreen(m_hWnd, &pt);

    RECT barClient{};
    GetClientRect(m_hWnd, &barClient);
    POINT ptLeft{ barClient.left, 0 };
    POINT ptRight{ barClient.right, 0 };
    ClientToScreen(m_hWnd, &ptLeft);
    ClientToScreen(m_hWnd, &ptRight);

    int tipX = pt.x - tipW / 2;
    int tipY = pt.y - tipH - tipGap;

    const int margin = FontHelper::ScaleDipToPx(6, dpi);
    if (ptRight.x - ptLeft.x >= tipW + margin * 2)
    {
        tipX = std::clamp(tipX, static_cast<int>(ptLeft.x) + margin,
            static_cast<int>(ptRight.x) - tipW - margin);
    }

    HMONITOR hMonitor = MonitorFromWindow(ownerHwnd ? ownerHwnd : m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMonitor, &mi))
    {
        tipX = std::clamp(tipX, static_cast<int>(mi.rcMonitor.left),
            static_cast<int>(mi.rcMonitor.right) - tipW);
        if (tipY < mi.rcMonitor.top)
        {
            tipY = pt.y + barH + tipGap;
        }
        tipY = std::clamp(tipY, static_cast<int>(mi.rcMonitor.top),
            static_cast<int>(mi.rcMonitor.bottom) - tipH);
    }

    const bool ownerTopMost = ownerHwnd && (GetWindowLongPtrW(ownerHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    SetWindowPos(
        m_seekTooltipHwnd,
        ownerTopMost ? HWND_TOPMOST : HWND_TOP,
        tipX,
        tipY,
        tipW,
        tipH,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    InvalidateRect(m_seekTooltipHwnd, nullptr, FALSE);
}

void ControlBarView::HideSeekTooltip()
{
    if (m_seekTooltipHwnd && IsWindow(m_seekTooltipHwnd) && IsWindowVisible(m_seekTooltipHwnd))
    {
        ShowWindow(m_seekTooltipHwnd, SW_HIDE);
    }
}

void ControlBarView::DrawSeekTooltip(HDC hdc)
{
    if (!m_seekTooltipHwnd || !IsWindow(m_seekTooltipHwnd) || !hdc)
    {
        return;
    }

    if (FAILED(CreateDeviceResources()))
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_seekTooltipHwnd, &rc);
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    if (FAILED(m_renderTarget->BindDC(hdc, &rc)))
    {
        DiscardDeviceResources();
        return;
    }

    const float dpi = static_cast<float>(FontHelper::GetWindowDpi(m_seekTooltipHwnd));
    m_renderTarget->SetDpi(dpi, dpi);
    m_renderTarget->BeginDraw();

    const D2D1_SIZE_F rtSize = m_renderTarget->GetSize();
    m_renderTarget->Clear(D2D1::ColorF(0x1B1E28, 1.0f));
    m_renderTarget->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, rtSize.width - 0.5f, rtSize.height - 0.5f), 6.0f, 6.0f),
        m_accentBrush.Get(),
        1.0f);

    if (!m_seekTooltipText.empty())
    {
        m_renderTarget->DrawTextW(
            m_seekTooltipText.c_str(),
            static_cast<UINT32>(m_seekTooltipText.length()),
            m_speedTextFormat.Get(),
            D2D1::RectF(4.0f, 0.0f, rtSize.width - 4.0f, rtSize.height),
            m_barTextBrush.Get());
    }

    if (m_renderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

LRESULT CALLBACK ControlBarView::SeekTooltipWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ControlBarView* view = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<ControlBarView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_seekTooltipHwnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<ControlBarView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleSeekTooltipMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT ControlBarView::HandleSeekTooltipMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawSeekTooltip(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCDESTROY:
        if (m_seekTooltipHwnd == hWnd)
        {
            m_seekTooltipHwnd = nullptr;
        }
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
