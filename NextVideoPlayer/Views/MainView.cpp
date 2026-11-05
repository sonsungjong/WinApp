#include "Views/MainView.h"
#include "Views/FontHelper.h"
#include "Resource.h"
#include <algorithm>
#include <filesystem>

namespace
{
    constexpr int kMaxLoadString = 100;
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 760;
    constexpr int kControlBarHeight = IControlBarView::kDefaultHeight;

    constexpr wchar_t kVideoHostClassName[] = L"NextVideoPlayer_VideoHostWnd";
    constexpr wchar_t kPlaylistClassName[] = L"NextVideoPlayer_PlaylistWnd";

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

    constexpr int IDC_PL_LIST = 2001;
    constexpr int IDC_PL_PLAY = 2002;
    constexpr int IDC_PL_ADD = 2003;
    constexpr int IDC_PL_REMOVE = 2004;
    constexpr int IDC_PL_CLEAR = 2005;
}

MainView::MainView(IControlBarView& controlBarView)
    : m_controlBarView(controlBarView)
{
}

bool MainView::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    WCHAR titleBuffer[kMaxLoadString]{};
    WCHAR classBuffer[kMaxLoadString]{};
    LoadStringW(hInstance, IDS_APP_TITLE, titleBuffer, kMaxLoadString);
    LoadStringW(hInstance, IDC_NEXTVIDEOPLAYER, classBuffer, kMaxLoadString);

    m_defaultTitle = titleBuffer;
    m_windowClass = classBuffer;

    RegisterWindowClass(hInstance);
    RegisterHelperClasses(hInstance);

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        m_windowClass.c_str(),
        m_defaultTitle.c_str(),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        hInstance,
        this);

    if (!m_hWnd)
    {
        return false;
    }

    m_hMenu = GetMenu(m_hWnd);
    m_lastMouseActiveTick = GetTickCount64();

    RECT clientRect{};
    GetClientRect(m_hWnd, &clientRect);
    int clientW = clientRect.right - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top;
    int videoH = std::max(0, clientH - kControlBarHeight);

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

    if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_taskbarList))))
    {
        m_taskbarList->HrInit();
    }

    if (!m_controlBarView.Initialize(hInstance, m_hWnd, 0, videoH, clientW, kControlBarHeight))
    {
        return false;
    }

    SetWindowPos(m_videoHwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return true;
}

void MainView::Show(int cmdShow)
{
    if (m_hWnd)
    {
        ShowWindow(m_hWnd, cmdShow);
        UpdateWindow(m_hWnd);
    }
}

HINSTANCE MainView::GetInstance() const
{
    return m_hInstance;
}

HWND MainView::GetHwnd() const
{
    return m_hWnd;
}

HWND MainView::GetVideoHwnd() const
{
    return m_videoHwnd;
}

void MainView::SetVideoVisible(bool visible)
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

void MainView::SetWindowTitle(const std::wstring& title)
{
    std::wstring targetTitle = title.empty() ? m_defaultTitle : title;
    if (m_currentTitle == targetTitle)
    {
        return;
    }

    m_currentTitle = targetTitle;
    if (m_hWnd)
    {
        SetWindowTextW(m_hWnd, m_currentTitle.c_str());
    }
}

void MainView::SetStatusText(const std::wstring& statusText)
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

void MainView::UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost)
{
    bool changed = !m_hasMenuState ||
        (m_menuRepeat != repeatEnabled) ||
        (m_menuSubVisible != subtitleVisible) ||
        (m_menuTopMost != topMost);

    m_menuRepeat = repeatEnabled;
    m_menuSubVisible = subtitleVisible;
    m_menuTopMost = topMost;
    m_hasMenuState = true;

    if (m_hMenu && changed)
    {
        CheckMenuItem(m_hMenu, IDM_REPEAT_TOGGLE, MF_BYCOMMAND | (repeatEnabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_hMenu, IDM_SUB_TOGGLE, MF_BYCOMMAND | (subtitleVisible ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_hMenu, IDM_VIEW_TOPMOST, MF_BYCOMMAND | (topMost ? MF_CHECKED : MF_UNCHECKED));
    }
}

void MainView::UpdatePlaylistView(const std::vector<std::wstring>& items, int currentIndex)
{
    m_cachedPlaylist = items;
    m_cachedPlaylistIndex = currentIndex;

    if (!m_playlistListBox || !IsWindow(m_playlistListBox))
    {
        return;
    }

    SendMessageW(m_playlistListBox, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < items.size(); ++i)
    {
        std::filesystem::path p(items[i]);
        std::wstring entry = std::to_wstring(i + 1) + L". " + p.filename().wstring();
        if (static_cast<int>(i) == currentIndex)
        {
            entry = L"▶ " + entry;
        }
        SendMessageW(m_playlistListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.c_str()));
    }

    if (currentIndex >= 0 && currentIndex < static_cast<int>(items.size()))
    {
        SendMessageW(m_playlistListBox, LB_SETCURSEL, currentIndex, 0);
    }
}

bool MainView::IsPlaylistVisible() const
{
    return m_playlistHwnd && IsWindow(m_playlistHwnd) && IsWindowVisible(m_playlistHwnd);
}

void MainView::SetTopMost(bool topMost)
{
    m_topMost = topMost;
    if (m_hWnd)
    {
        SetWindowPos(
            m_hWnd,
            (m_isFullscreen || topMost) ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void MainView::ToggleFullscreen()
{
    if (!m_hWnd)
    {
        return;
    }

    if (!m_isFullscreen)
    {
        m_savedStyle = GetWindowLongPtrW(m_hWnd, GWL_STYLE);
        m_savedExStyle = GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
        GetWindowPlacement(m_hWnd, &m_savedPlacement);

        HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(MONITORINFO) };
        GetMonitorInfoW(hMonitor, &mi);

        int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
        int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

        m_isFullscreen = true;
        m_lastMouseActiveTick = GetTickCount64();

        m_controlBarView.SetVisible(false);

        SetMenu(m_hWnd, nullptr);
        SetWindowLongPtrW(
            m_hWnd,
            GWL_STYLE,
            (m_savedStyle & ~(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE | WS_MINIMIZE)) | WS_CLIPCHILDREN | WS_VISIBLE);
        SetWindowLongPtrW(
            m_hWnd,
            GWL_EXSTYLE,
            m_savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        SetWindowPos(
            m_hWnd,
            HWND_TOPMOST,
            mi.rcMonitor.left,
            mi.rcMonitor.top,
            screenW,
            screenH,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        if (m_taskbarList)
        {
            m_taskbarList->MarkFullscreenWindow(m_hWnd, TRUE);
        }

        UpdateLayout();
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hWnd);
        RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
    }
    else
    {
        ExitFullscreen();
    }
}

void MainView::ExitFullscreen()
{
    if (!m_hWnd || !m_isFullscreen)
    {
        return;
    }

    m_isFullscreen = false;

    if (m_taskbarList)
    {
        m_taskbarList->MarkFullscreenWindow(m_hWnd, FALSE);
    }

    SetMenu(m_hWnd, m_hMenu);
    SetWindowLongPtrW(m_hWnd, GWL_STYLE, m_savedStyle);
    SetWindowLongPtrW(m_hWnd, GWL_EXSTYLE, m_savedExStyle);
    SetWindowPlacement(m_hWnd, &m_savedPlacement);
    SetWindowPos(
        m_hWnd,
        m_topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_controlBarView.SetVisible(true);
    UpdateLayout();

    RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
}

bool MainView::IsFullscreen() const
{
    return m_isFullscreen;
}

void MainView::UpdateFullscreenAutoHide()
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

    HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(MONITORINFO) };
    GetMonitorInfoW(hMonitor, &mi);

    bool inMonitor = (cursorPt.x >= mi.rcMonitor.left && cursorPt.x < mi.rcMonitor.right &&
                      cursorPt.y >= mi.rcMonitor.top && cursorPt.y < mi.rcMonitor.bottom);
    bool inToolbarZone = inMonitor && (cursorPt.y >= mi.rcMonitor.bottom - kControlBarHeight);

    if (inToolbarZone || m_controlBarView.IsDragging())
    {
        m_lastMouseActiveTick = GetTickCount64();

        if (!m_controlBarView.IsVisible())
        {
            RECT rc{};
            GetClientRect(m_hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            if (GetForegroundWindow() == m_hWnd)
            {
                SetWindowPos(
                    m_hWnd,
                    HWND_TOPMOST,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }

            m_controlBarView.SetBounds(0, h - kControlBarHeight, w, kControlBarHeight, true);
            HWND barHwnd = m_controlBarView.GetHwnd();
            if (barHwnd)
            {
                RedrawWindow(
                    barHwnd,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            }
        }
    }
    else
    {
        if (m_controlBarView.IsVisible() && (GetTickCount64() - m_lastMouseActiveTick >= 200))
        {
            m_controlBarView.SetVisible(false);
            if (m_isVideoVisible && m_videoHwnd)
            {
                RedrawWindow(
                    m_videoHwnd,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
            }
            else
            {
                RedrawWindow(
                    m_hWnd,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
            }
        }
    }
}

std::wstring MainView::PromptOpenVideoFile()
{
    WCHAR fileBuffer[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"동영상 파일 (*.mp4;*.mkv;*.avi;*.wmv;*.mov;*.webm;*.flv;*.ts)\0*.mp4;*.mkv;*.avi;*.wmv;*.mov;*.webm;*.flv;*.ts\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn))
    {
        return fileBuffer;
    }

    return {};
}

std::wstring MainView::PromptOpenSubtitleFile()
{
    WCHAR fileBuffer[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
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

std::wstring MainView::PromptSaveScreenshotFile()
{
    WCHAR fileBuffer[MAX_PATH] = L"capture.png";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
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

void MainView::TogglePlaylistWindow()
{
    if (m_playlistHwnd && IsWindow(m_playlistHwnd))
    {
        if (IsWindowVisible(m_playlistHwnd))
        {
            ShowWindow(m_playlistHwnd, SW_HIDE);
        }
        else
        {
            ShowWindow(m_playlistHwnd, SW_SHOW);
            UpdatePlaylistView(m_cachedPlaylist, m_cachedPlaylistIndex);
        }
        if (m_onPlaylistVisibilityChanged)
        {
            m_onPlaylistVisibilityChanged();
        }
        return;
    }

    RECT parentRc{};
    GetWindowRect(m_hWnd, &parentRc);

    m_playlistHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kPlaylistClassName,
        L"재생목록 (Playlist)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        parentRc.right - 360,
        parentRc.top + 60,
        340,
        420,
        m_hWnd,
        nullptr,
        m_hInstance,
        this);

    UpdatePlaylistView(m_cachedPlaylist, m_cachedPlaylistIndex);
    if (m_onPlaylistVisibilityChanged)
    {
        m_onPlaylistVisibilityChanged();
    }
}

void MainView::Render()
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

void MainView::RenderMainPaint(HDC hdc)
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

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void MainView::SetOnOpenFileRequested(std::function<void()> handler) { m_onOpenFileRequested = std::move(handler); }
void MainView::SetOnOpenSubtitleRequested(std::function<void()> handler) { m_onOpenSubtitleRequested = std::move(handler); }
void MainView::SetOnScreenshotRequested(std::function<void()> handler) { m_onScreenshotRequested = std::move(handler); }
void MainView::SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) { m_onFilesDropped = std::move(handler); }
void MainView::SetOnPlayPauseRequested(std::function<void()> handler) { m_onPlayPauseRequested = std::move(handler); }
void MainView::SetOnStopRequested(std::function<void()> handler) { m_onStopRequested = std::move(handler); }
void MainView::SetOnPrevRequested(std::function<void()> handler) { m_onPrevRequested = std::move(handler); }
void MainView::SetOnNextRequested(std::function<void()> handler) { m_onNextRequested = std::move(handler); }
void MainView::SetOnSeekDirection(std::function<void(int)> handler) { m_onSeekDirection = std::move(handler); }
void MainView::SetOnVolumeStep(std::function<void(int)> handler) { m_onVolumeStep = std::move(handler); }
void MainView::SetOnRepeatToggled(std::function<void()> handler) { m_onRepeatToggled = std::move(handler); }
void MainView::SetOnABLoopAction(std::function<void(int)> handler) { m_onABLoopAction = std::move(handler); }
void MainView::SetOnSubtitleToggle(std::function<void()> handler) { m_onSubtitleToggle = std::move(handler); }
void MainView::SetOnSubtitleDelayStep(std::function<void(double)> handler) { m_onSubtitleDelayStep = std::move(handler); }
void MainView::SetOnSubtitleScaleStep(std::function<void(double)> handler) { m_onSubtitleScaleStep = std::move(handler); }
void MainView::SetOnSubtitlePosStep(std::function<void(int)> handler) { m_onSubtitlePosStep = std::move(handler); }
void MainView::SetOnTopMostToggled(std::function<void()> handler) { m_onTopMostToggled = std::move(handler); }
void MainView::SetOnSettingsRequested(std::function<void()> handler) { m_onSettingsRequested = std::move(handler); }
void MainView::SetOnPlaylistPlayIndex(std::function<void(int)> handler) { m_onPlaylistPlayIndex = std::move(handler); }
void MainView::SetOnPlaylistAddFiles(std::function<void()> handler) { m_onPlaylistAddFiles = std::move(handler); }
void MainView::SetOnPlaylistRemoveIndex(std::function<void(int)> handler) { m_onPlaylistRemoveIndex = std::move(handler); }
void MainView::SetOnPlaylistClear(std::function<void()> handler) { m_onPlaylistClear = std::move(handler); }
void MainView::SetOnPlaylistVisibilityChanged(std::function<void()> handler) { m_onPlaylistVisibilityChanged = std::move(handler); }

ATOM MainView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = MainView::WindowProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NEXTVIDEOPLAYER);
    wcex.lpszClassName = m_windowClass.c_str();
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));

    return RegisterClassExW(&wcex);
}

void MainView::RegisterHelperClasses(HINSTANCE hInstance) const
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

    WNDCLASSEXW playlistWc{};
    playlistWc.cbSize = sizeof(WNDCLASSEXW);
    playlistWc.lpfnWndProc = MainView::PlaylistWndProc;
    playlistWc.hInstance = hInstance;
    playlistWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    playlistWc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    playlistWc.lpszClassName = kPlaylistClassName;
    RegisterClassExW(&playlistWc);
}

HRESULT MainView::CreateDeviceIndependentResources()
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

HRESULT MainView::CreateDeviceResources()
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
        return hr;
    }

    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.10f), m_mainBgBrush.GetAddressOf());
    return m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.88f, 0.88f, 0.88f), m_mainTextBrush.GetAddressOf());
}

void MainView::DiscardDeviceResources()
{
    m_mainTextBrush.Reset();
    m_mainBgBrush.Reset();
    m_renderTarget.Reset();
}

void MainView::UpdateLayout()
{
    if (!m_hWnd)
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (m_isFullscreen)
    {
        m_controlBarView.SetBounds(0, h - kControlBarHeight, w, kControlBarHeight, m_controlBarView.IsVisible());

        if (m_videoHwnd)
        {
            SetWindowPos(
                m_videoHwnd,
                HWND_BOTTOM,
                0,
                0,
                w,
                h,
                SWP_NOACTIVATE);
        }
    }
    else
    {
        int videoH = std::max(0, h - kControlBarHeight);
        m_controlBarView.SetBounds(0, videoH, w, kControlBarHeight, true);

        if (m_videoHwnd)
        {
            SetWindowPos(
                m_videoHwnd,
                HWND_BOTTOM,
                0,
                0,
                w,
                videoH,
                SWP_NOACTIVATE);
        }
    }
}

LRESULT CALLBACK MainView::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainView* view = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<MainView*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_hWnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<MainView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK MainView::PlaylistWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainView* view = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<MainView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    else
    {
        view = reinterpret_cast<MainView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandlePlaylistMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT MainView::HandlePlaylistMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HFONT hFont = FontHelper::GetNanumDialogFont();
        m_playlistListBox = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"LISTBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
            10, 10, 304, 320,
            hWnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PL_LIST)),
            m_hInstance,
            nullptr);
        SendMessageW(m_playlistListBox, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        HWND hPlay = CreateWindowExW(0, L"BUTTON", L"선택 재생", WS_CHILD | WS_VISIBLE,
            10, 340, 72, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PL_PLAY)), m_hInstance, nullptr);
        SendMessageW(hPlay, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        HWND hAdd = CreateWindowExW(0, L"BUTTON", L"파일 추가", WS_CHILD | WS_VISIBLE,
            87, 340, 72, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PL_ADD)), m_hInstance, nullptr);
        SendMessageW(hAdd, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        HWND hRemove = CreateWindowExW(0, L"BUTTON", L"선택 삭제", WS_CHILD | WS_VISIBLE,
            164, 340, 72, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PL_REMOVE)), m_hInstance, nullptr);
        SendMessageW(hRemove, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        HWND hClear = CreateWindowExW(0, L"BUTTON", L"비우기", WS_CHILD | WS_VISIBLE,
            241, 340, 72, 28, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PL_CLEAR)), m_hInstance, nullptr);
        SendMessageW(hClear, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if ((id == IDC_PL_LIST && code == LBN_DBLCLK) || id == IDC_PL_PLAY)
        {
            int sel = static_cast<int>(SendMessageW(m_playlistListBox, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR && m_onPlaylistPlayIndex)
            {
                m_onPlaylistPlayIndex(sel);
            }
            return 0;
        }
        if (id == IDC_PL_ADD && m_onPlaylistAddFiles)
        {
            m_onPlaylistAddFiles();
            return 0;
        }
        if (id == IDC_PL_REMOVE && m_onPlaylistRemoveIndex)
        {
            int sel = static_cast<int>(SendMessageW(m_playlistListBox, LB_GETCURSEL, 0, 0));
            if (sel != LB_ERR)
            {
                m_onPlaylistRemoveIndex(sel);
            }
            return 0;
        }
        if (id == IDC_PL_CLEAR && m_onPlaylistClear)
        {
            m_onPlaylistClear();
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        if (m_onPlaylistVisibilityChanged)
        {
            m_onPlaylistVisibilityChanged();
        }
        return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT MainView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (m_onVolumeStep)
        {
            m_onVolumeStep(delta > 0 ? 5 : -5);
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        ToggleFullscreen();
        return 0;
    }

    case WM_ACTIVATEAPP:
    {
        if (m_isFullscreen && m_hWnd)
        {
            if (wParam == TRUE)
            {
                SetWindowPos(
                    m_hWnd,
                    HWND_TOPMOST,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
            else if (!(m_savedExStyle & WS_EX_TOPMOST))
            {
                SetWindowPos(
                    m_hWnd,
                    HWND_NOTOPMOST,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
        }
        break;
    }

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
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

    case WM_DESTROY:
        DiscardDeviceResources();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

void MainView::OnResize(UINT, UINT)
{
    UpdateLayout();
    Render();
}

void MainView::OnCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_FILE_OPEN:
        if (m_onOpenFileRequested) m_onOpenFileRequested();
        break;
    case IDM_SUBTITLE_OPEN:
        if (m_onOpenSubtitleRequested) m_onOpenSubtitleRequested();
        break;
    case IDM_SCREENSHOT:
        if (m_onScreenshotRequested) m_onScreenshotRequested();
        break;
    case IDM_PLAY_PAUSE:
        if (m_onPlayPauseRequested) m_onPlayPauseRequested();
        break;
    case IDM_STOP:
        if (m_onStopRequested) m_onStopRequested();
        break;
    case IDM_PREV_FILE:
        if (m_onPrevRequested) m_onPrevRequested();
        break;
    case IDM_NEXT_FILE:
        if (m_onNextRequested) m_onNextRequested();
        break;
    case IDM_REPEAT_TOGGLE:
        if (m_onRepeatToggled) m_onRepeatToggled();
        break;
    case IDM_AB_SET_A:
        if (m_onABLoopAction) m_onABLoopAction(1);
        break;
    case IDM_AB_SET_B:
        if (m_onABLoopAction) m_onABLoopAction(2);
        break;
    case IDM_AB_CLEAR:
        if (m_onABLoopAction) m_onABLoopAction(0);
        break;
    case IDM_SUB_TOGGLE:
        if (m_onSubtitleToggle) m_onSubtitleToggle();
        break;
    case IDM_SUB_DELAY_MINUS:
        if (m_onSubtitleDelayStep) m_onSubtitleDelayStep(-0.5);
        break;
    case IDM_SUB_DELAY_PLUS:
        if (m_onSubtitleDelayStep) m_onSubtitleDelayStep(0.5);
        break;
    case IDM_SUB_SCALE_PLUS:
        if (m_onSubtitleScaleStep) m_onSubtitleScaleStep(0.1);
        break;
    case IDM_SUB_SCALE_MINUS:
        if (m_onSubtitleScaleStep) m_onSubtitleScaleStep(-0.1);
        break;
    case IDM_SUB_POS_UP:
        if (m_onSubtitlePosStep) m_onSubtitlePosStep(-2);
        break;
    case IDM_SUB_POS_DOWN:
        if (m_onSubtitlePosStep) m_onSubtitlePosStep(2);
        break;
    case IDM_VIEW_FULLSCREEN:
        ToggleFullscreen();
        break;
    case IDM_VIEW_TOPMOST:
        if (m_onTopMostToggled) m_onTopMostToggled();
        break;
    case IDM_VIEW_PLAYLIST:
        TogglePlaylistWindow();
        break;
    case IDM_VIEW_SETTINGS:
        if (m_onSettingsRequested) m_onSettingsRequested();
        break;
    case IDM_EXIT:
        DestroyWindow(m_hWnd);
        break;
    }
}

void MainView::OnDropFiles(HDROP hDrop)
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

void MainView::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case VK_SPACE:
        if (m_onPlayPauseRequested) m_onPlayPauseRequested();
        break;
    case VK_LEFT:
        if (m_onSeekDirection) m_onSeekDirection(-1);
        break;
    case VK_RIGHT:
        if (m_onSeekDirection) m_onSeekDirection(1);
        break;
    case VK_UP:
        if (m_onVolumeStep) m_onVolumeStep(5);
        break;
    case VK_DOWN:
        if (m_onVolumeStep) m_onVolumeStep(-5);
        break;
    case VK_RETURN:
        ToggleFullscreen();
        break;
    case VK_ESCAPE:
        if (m_isFullscreen) ExitFullscreen();
        break;
    case VK_OEM_4: // '['
        if (m_onABLoopAction) m_onABLoopAction(1);
        break;
    case VK_OEM_6: // ']'
        if (m_onABLoopAction) m_onABLoopAction(2);
        break;
    case VK_OEM_5: // '\'
        if (m_onABLoopAction) m_onABLoopAction(0);
        break;
    }
}
