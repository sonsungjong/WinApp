#include "Views/VideoPlayerView.h"
#include "Views/FileDialogs.h"
#include "Views/FontHelper.h"
#include "Resource.h"
#include <algorithm>
#include <filesystem>

namespace
{
    constexpr int kControlBarHeight = IControlBarView::kDefaultHeight;
    constexpr wchar_t kPlayerClassName[] = L"NextVideoPlayer_VideoPlayerWnd";
    constexpr wchar_t kVideoHostClassName[] = L"NextVideoPlayer_VideoHostWnd";

    LRESULT CALLBACK VideoHostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            if (hdc)
            {
                FillRect(hdc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

VideoPlayerView::VideoPlayerView(IControlBarView& controlBarView)
    : m_controlBarView(controlBarView)
{
}

VideoPlayerView::~VideoPlayerView()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        DestroyWindow(m_hWnd);
    }
}

bool VideoPlayerView::Initialize(HINSTANCE hInstance, HWND parentHwnd)
{
    m_hInstance = hInstance;
    m_parentHwnd = parentHwnd;
    m_windowClass = kPlayerClassName;

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    RegisterWindowClass(hInstance);
    RegisterHelperClasses(hInstance);

    RECT clientRect{};
    GetClientRect(parentHwnd, &clientRect);
    const int clientW = clientRect.right - clientRect.left;
    const int clientH = clientRect.bottom - clientRect.top;

    m_hWnd = CreateWindowExW(
        0,
        m_windowClass.c_str(),
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        clientW,
        clientH,
        parentHwnd,
        nullptr,
        hInstance,
        this);

    if (!m_hWnd)
    {
        return false;
    }

    m_lastMouseActiveTick = GetTickCount64();

    const int videoH = (std::max)(0, clientH - kControlBarHeight);
    m_videoHwnd = CreateWindowExW(
        0,
        kVideoHostClassName,
        nullptr,
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        clientW,
        videoH,
        m_hWnd,
        nullptr,
        hInstance,
        nullptr);

    if (!m_videoHwnd)
    {
        return false;
    }

    EnableWindow(m_videoHwnd, FALSE);

    if (!m_controlBarView.Initialize(hInstance, m_hWnd, 0, videoH, clientW, kControlBarHeight))
    {
        return false;
    }

    SetWindowPos(m_videoHwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return true;
}

HINSTANCE VideoPlayerView::GetInstance() const { return m_hInstance; }
HWND VideoPlayerView::GetHwnd() const { return m_hWnd; }
HWND VideoPlayerView::GetVideoHwnd() const { return m_videoHwnd; }

HWND VideoPlayerView::GetOwnerHwnd() const
{
    return m_parentHwnd ? m_parentHwnd : m_hWnd;
}

void VideoPlayerView::SetBounds(int x, int y, int width, int height)
{
    if (!m_hWnd)
    {
        return;
    }
    SetWindowPos(m_hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateLayout();
}

void VideoPlayerView::SetVideoVisible(bool visible)
{
    if (m_isVideoVisible == visible)
    {
        return;
    }
    m_isVideoVisible = visible;
    if (m_videoHwnd)
    {
        ShowWindow(m_videoHwnd, visible ? SW_SHOW : SW_HIDE);
    }
    UpdateLayout();
    Render();
}

void VideoPlayerView::SetStatusText(const std::wstring& statusText)
{
    if (m_statusText == statusText)
    {
        return;
    }
    m_statusText = statusText;
    if (!m_isVideoVisible && m_hWnd)
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void VideoPlayerView::SetFullscreenActive(bool active)
{
    if (m_isFullscreen == active)
    {
        return;
    }
    m_isFullscreen = active;
    m_lastMouseActiveTick = GetTickCount64();
    m_controlBarView.SetVisible(!m_isFullscreen);
    UpdateLayout();
}

bool VideoPlayerView::IsFullscreenActive() const
{
    return m_isFullscreen;
}

void VideoPlayerView::UpdateFullscreenAutoHide()
{
    if (!m_hWnd || !m_isFullscreen)
    {
        return;
    }

    POINT cursorPt{};
    if (!GetCursorPos(&cursorPt))
    {
        return;
    }

    HWND shell = GetOwnerHwnd();
    HMONITOR hMonitor = MonitorFromWindow(shell ? shell : m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(MONITORINFO) };
    GetMonitorInfoW(hMonitor, &mi);

    const bool inMonitor = (cursorPt.x >= mi.rcMonitor.left && cursorPt.x < mi.rcMonitor.right &&
                            cursorPt.y >= mi.rcMonitor.top && cursorPt.y < mi.rcMonitor.bottom);
    const bool inToolbarZone = inMonitor && (cursorPt.y >= mi.rcMonitor.bottom - kControlBarHeight);

    if (inToolbarZone || m_controlBarView.IsDragging())
    {
        m_lastMouseActiveTick = GetTickCount64();
        if (!m_controlBarView.IsVisible())
        {
            RECT rc{};
            GetClientRect(m_hWnd, &rc);
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            m_controlBarView.SetBounds(0, h - kControlBarHeight, w, kControlBarHeight, true);
            if (HWND barHwnd = m_controlBarView.GetHwnd())
            {
                RedrawWindow(barHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            }
        }
    }
    else if (m_controlBarView.IsVisible() && (GetTickCount64() - m_lastMouseActiveTick >= 200))
    {
        m_controlBarView.SetVisible(false);
        if (m_isVideoVisible && m_videoHwnd)
        {
            RedrawWindow(m_videoHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
        }
        else
        {
            RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
        }
    }
}

std::vector<std::wstring> VideoPlayerView::PromptOpenVideoFiles()
{
    return FileDialogs::OpenVideoFiles(GetOwnerHwnd());
}

std::wstring VideoPlayerView::PromptOpenSubtitleFile()
{
    WCHAR fileBuffer[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetOwnerHwnd();
    ofn.lpstrFilter = L"자막 파일 (*.smi;*.srt;*.ass;*.ssa;*.vtt;*.sub)\0*.smi;*.srt;*.ass;*.ssa;*.vtt;*.sub\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn))
    {
        return fileBuffer;
    }
    return {};
}

std::wstring VideoPlayerView::PromptSaveScreenshotFile()
{
    WCHAR fileBuffer[MAX_PATH] = L"capture.png";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetOwnerHwnd();
    ofn.lpstrFilter = L"PNG 이미지 (*.png)\0*.png\0JPEG 이미지 (*.jpg)\0*.jpg\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn))
    {
        return fileBuffer;
    }
    return {};
}

void VideoPlayerView::Render()
{
    if (!m_hWnd)
    {
        return;
    }
    if (!m_isVideoVisible)
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
    m_controlBarView.Invalidate();
}

void VideoPlayerView::RenderMainPaint(HDC hdc)
{
    if (!m_hWnd || !hdc)
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

    m_renderTarget->BeginDraw();
    D2D1_SIZE_F size = m_renderTarget->GetSize();
    if (!m_isVideoVisible)
    {
        D2D1_RECT_F videoRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
        m_renderTarget->FillRectangle(videoRect, m_mainBgBrush.Get());
        if (!m_statusText.empty() && m_textFormat && m_mainTextBrush)
        {
            m_renderTarget->DrawTextW(
                m_statusText.c_str(),
                static_cast<UINT32>(m_statusText.length()),
                m_textFormat.Get(),
                videoRect,
                m_mainTextBrush.Get());
        }
    }
    else
    {
        m_renderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));
    }

    if (m_renderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void VideoPlayerView::SetOnPlaylistRequested(std::function<void()> handler) { m_onPlaylistRequested = std::move(handler); }
void VideoPlayerView::SetOnOpenFileRequested(std::function<void()> handler) { m_onOpenFileRequested = std::move(handler); }
void VideoPlayerView::SetOnOpenSubtitleRequested(std::function<void()> handler) { m_onOpenSubtitleRequested = std::move(handler); }
void VideoPlayerView::SetOnScreenshotRequested(std::function<void()> handler) { m_onScreenshotRequested = std::move(handler); }
void VideoPlayerView::SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) { m_onFilesDropped = std::move(handler); }
void VideoPlayerView::SetOnPlayPauseRequested(std::function<void()> handler) { m_onPlayPauseRequested = std::move(handler); }
void VideoPlayerView::SetOnStopRequested(std::function<void()> handler) { m_onStopRequested = std::move(handler); }
void VideoPlayerView::SetOnPrevRequested(std::function<void()> handler) { m_onPrevRequested = std::move(handler); }
void VideoPlayerView::SetOnNextRequested(std::function<void()> handler) { m_onNextRequested = std::move(handler); }
void VideoPlayerView::SetOnSeekDirection(std::function<void(int)> handler) { m_onSeekDirection = std::move(handler); }
void VideoPlayerView::SetOnVolumeStep(std::function<void(int)> handler) { m_onVolumeStep = std::move(handler); }
void VideoPlayerView::SetOnRepeatToggled(std::function<void()> handler) { m_onRepeatToggled = std::move(handler); }
void VideoPlayerView::SetOnABLoopAction(std::function<void(int)> handler) { m_onABLoopAction = std::move(handler); }
void VideoPlayerView::SetOnSubtitleToggle(std::function<void()> handler) { m_onSubtitleToggle = std::move(handler); }
void VideoPlayerView::SetOnSubtitleDelayStep(std::function<void(double)> handler) { m_onSubtitleDelayStep = std::move(handler); }
void VideoPlayerView::SetOnSubtitleScaleStep(std::function<void(double)> handler) { m_onSubtitleScaleStep = std::move(handler); }
void VideoPlayerView::SetOnSubtitlePosStep(std::function<void(int)> handler) { m_onSubtitlePosStep = std::move(handler); }
void VideoPlayerView::SetOnTopMostToggled(std::function<void()> handler) { m_onTopMostToggled = std::move(handler); }
void VideoPlayerView::SetOnSettingsRequested(std::function<void()> handler) { m_onSettingsRequested = std::move(handler); }
void VideoPlayerView::SetOnFullscreenToggleRequested(std::function<void()> handler) { m_onFullscreenToggleRequested = std::move(handler); }

void VideoPlayerView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = VideoPlayerView::WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = m_windowClass.c_str();
    RegisterClassExW(&wcex);
}

void VideoPlayerView::RegisterHelperClasses(HINSTANCE hInstance) const
{
    WNDCLASSEXW videoWc{};
    videoWc.cbSize = sizeof(WNDCLASSEXW);
    videoWc.style = CS_HREDRAW | CS_VREDRAW;
    videoWc.lpfnWndProc = VideoHostWndProc;
    videoWc.hInstance = hInstance;
    videoWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    videoWc.hbrBackground = nullptr;
    videoWc.lpszClassName = kVideoHostClassName;
    RegisterClassExW(&videoWc);
}

HRESULT VideoPlayerView::CreateDeviceIndependentResources()
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

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName,
        m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        18.0f,
        L"ko-kr",
        m_textFormat.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }
    m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return S_OK;
}

HRESULT VideoPlayerView::CreateDeviceResources()
{
    if (m_renderTarget)
    {
        return S_OK;
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

    if (FAILED(hr = m_renderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.08f, 0.08f, 0.10f), m_mainBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.88f, 0.88f, 0.88f), m_mainTextBrush.GetAddressOf())))
    {
        DiscardDeviceResources();
        return hr;
    }
    return S_OK;
}

void VideoPlayerView::DiscardDeviceResources()
{
    m_mainTextBrush.Reset();
    m_mainBgBrush.Reset();
    m_renderTarget.Reset();
}

void VideoPlayerView::UpdateLayout()
{
    if (!m_hWnd)
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;

    if (m_isFullscreen)
    {
        m_controlBarView.SetBounds(0, h - kControlBarHeight, w, kControlBarHeight, m_controlBarView.IsVisible());
        if (m_videoHwnd)
        {
            SetWindowPos(m_videoHwnd, HWND_BOTTOM, 0, 0, w, h, SWP_NOACTIVATE);
        }
    }
    else
    {
        const int videoH = (std::max)(0, h - kControlBarHeight);
        m_controlBarView.SetBounds(0, videoH, w, kControlBarHeight, true);
        if (m_videoHwnd)
        {
            SetWindowPos(m_videoHwnd, HWND_BOTTOM, 0, 0, w, videoH, SWP_NOACTIVATE);
        }
    }
}

LRESULT CALLBACK VideoPlayerView::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    VideoPlayerView* view = nullptr;
    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<VideoPlayerView*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_hWnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<VideoPlayerView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleMessage(message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT VideoPlayerView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        OnResize();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(m_hWnd, &ps);
        RenderMainPaint(hdc);
        EndPaint(m_hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        if (m_onFullscreenToggleRequested)
        {
            m_onFullscreenToggleRequested();
        }
        return 0;
    case WM_DESTROY:
        DiscardDeviceResources();
        return 0;
    case WM_NCDESTROY:
    {
        HWND hwnd = m_hWnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hWnd = nullptr;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    }
    return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

void VideoPlayerView::OnResize()
{
    UpdateLayout();
    Render();
}

void VideoPlayerView::HandleCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_FILE_OPEN: if (m_onOpenFileRequested) m_onOpenFileRequested(); break;
    case IDM_SUBTITLE_OPEN: if (m_onOpenSubtitleRequested) m_onOpenSubtitleRequested(); break;
    case IDM_SCREENSHOT: if (m_onScreenshotRequested) m_onScreenshotRequested(); break;
    case IDM_PLAY_PAUSE: if (m_onPlayPauseRequested) m_onPlayPauseRequested(); break;
    case IDM_STOP: if (m_onStopRequested) m_onStopRequested(); break;
    case IDM_PREV_FILE: if (m_onPrevRequested) m_onPrevRequested(); break;
    case IDM_NEXT_FILE: if (m_onNextRequested) m_onNextRequested(); break;
    case IDM_REPEAT_TOGGLE: if (m_onRepeatToggled) m_onRepeatToggled(); break;
    case IDM_AB_SET_A: if (m_onABLoopAction) m_onABLoopAction(1); break;
    case IDM_AB_SET_B: if (m_onABLoopAction) m_onABLoopAction(2); break;
    case IDM_AB_CLEAR: if (m_onABLoopAction) m_onABLoopAction(0); break;
    case IDM_SUB_TOGGLE: if (m_onSubtitleToggle) m_onSubtitleToggle(); break;
    case IDM_SUB_DELAY_MINUS: if (m_onSubtitleDelayStep) m_onSubtitleDelayStep(-0.5); break;
    case IDM_SUB_DELAY_PLUS: if (m_onSubtitleDelayStep) m_onSubtitleDelayStep(0.5); break;
    case IDM_SUB_SCALE_PLUS: if (m_onSubtitleScaleStep) m_onSubtitleScaleStep(0.1); break;
    case IDM_SUB_SCALE_MINUS: if (m_onSubtitleScaleStep) m_onSubtitleScaleStep(-0.1); break;
    case IDM_SUB_POS_UP: if (m_onSubtitlePosStep) m_onSubtitlePosStep(-2); break;
    case IDM_SUB_POS_DOWN: if (m_onSubtitlePosStep) m_onSubtitlePosStep(2); break;
    case IDM_VIEW_TOPMOST: if (m_onTopMostToggled) m_onTopMostToggled(); break;
    case IDM_VIEW_PLAYLIST: if (m_onPlaylistRequested) m_onPlaylistRequested(); break;
    case IDM_VIEW_SETTINGS: if (m_onSettingsRequested) m_onSettingsRequested(); break;
    default: break;
    }
}

void VideoPlayerView::HandleKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_SPACE: if (m_onPlayPauseRequested) m_onPlayPauseRequested(); break;
    case VK_LEFT: if (m_onSeekDirection) m_onSeekDirection(-1); break;
    case VK_RIGHT: if (m_onSeekDirection) m_onSeekDirection(1); break;
    case VK_UP: if (m_onVolumeStep) m_onVolumeStep(5); break;
    case VK_DOWN: if (m_onVolumeStep) m_onVolumeStep(-5); break;
    case VK_OEM_4: if (m_onABLoopAction) m_onABLoopAction(1); break;
    case VK_OEM_6: if (m_onABLoopAction) m_onABLoopAction(2); break;
    case VK_OEM_5: if (m_onABLoopAction) m_onABLoopAction(0); break;
    default: break;
    }
}

void VideoPlayerView::HandleMouseWheel(int delta)
{
    if (m_onVolumeStep)
    {
        m_onVolumeStep(delta > 0 ? 5 : -5);
    }
}

void VideoPlayerView::HandleDropFiles(HDROP hDrop)
{
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> files;
    files.reserve(count);
    for (UINT i = 0; i < count; ++i)
    {
        WCHAR filePath[MAX_PATH]{};
        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH) > 0)
        {
            files.emplace_back(filePath);
        }
    }
    DragFinish(hDrop);
    if (!files.empty() && m_onFilesDropped)
    {
        m_onFilesDropped(files);
    }
}
