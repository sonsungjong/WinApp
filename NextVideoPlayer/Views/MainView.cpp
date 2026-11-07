#include "Views/MainView.h"
#include "Resource.h"

namespace
{
    constexpr int kMaxLoadString = 100;
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 760;

    // Windowed mode keeps a normal overlapped frame. Fullscreen uses WS_POPUP.
    constexpr DWORD kWindowedStyle =
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE;
    constexpr DWORD kFullscreenStyle = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN;
}

MainView::MainView() = default;

MainView::~MainView()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        DestroyWindow(m_hWnd);
    }
}

bool MainView::Initialize(HINSTANCE hInstance)
{
    m_hInstance = hInstance;

    WCHAR titleBuffer[kMaxLoadString]{};
    WCHAR classBuffer[kMaxLoadString]{};
    LoadStringW(hInstance, IDS_APP_TITLE, titleBuffer, kMaxLoadString);
    LoadStringW(hInstance, IDC_NEXTVIDEOPLAYER, classBuffer, kMaxLoadString);

    m_defaultTitle = titleBuffer;
    m_windowClass = classBuffer;
    RegisterWindowClass(hInstance);

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        m_windowClass.c_str(),
        m_defaultTitle.c_str(),
        kWindowedStyle & ~WS_VISIBLE,
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

    if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_taskbarList))))
    {
        m_taskbarList->HrInit();
    }

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

HINSTANCE MainView::GetInstance() const { return m_hInstance; }
HWND MainView::GetHwnd() const { return m_hWnd; }

void MainView::SetPlayerHwnd(HWND playerHwnd)
{
    m_playerHwnd = playerHwnd;
    LayoutPlayer();
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

void MainView::UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost)
{
    const bool changed = !m_hasMenuState ||
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

        const int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
        const int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

        m_isFullscreen = true;
        SetMenu(m_hWnd, nullptr);
        SetWindowLongPtrW(m_hWnd, GWL_STYLE, kFullscreenStyle);
        SetWindowLongPtrW(
            m_hWnd,
            GWL_EXSTYLE,
            (m_savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)) | WS_EX_TOPMOST);

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

        LayoutPlayer();
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hWnd);
        if (m_onFullscreenChanged)
        {
            m_onFullscreenChanged(true);
        }
        RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
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

    LayoutPlayer();
    if (m_onFullscreenChanged)
    {
        m_onFullscreenChanged(false);
    }
    RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
}

bool MainView::IsFullscreen() const
{
    return m_isFullscreen;
}

void MainView::SetOnTimer(std::function<void(UINT_PTR)> handler) { m_onTimer = std::move(handler); }
void MainView::SetOnCommand(std::function<void(int)> handler) { m_onCommand = std::move(handler); }
void MainView::SetOnKeyDown(std::function<void(WPARAM)> handler) { m_onKeyDown = std::move(handler); }
void MainView::SetOnMouseWheel(std::function<void(int)> handler) { m_onMouseWheel = std::move(handler); }
void MainView::SetOnFilesDropped(std::function<void(HDROP)> handler) { m_onFilesDropped = std::move(handler); }
void MainView::SetOnFullscreenChanged(std::function<void(bool)> handler) { m_onFullscreenChanged = std::move(handler); }
void MainView::SetOnActivationChanged(std::function<void(bool)> handler) { m_onActivationChanged = std::move(handler); }

ATOM MainView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = MainView::WindowProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NEXTVIDEOPLAYER);
    wcex.lpszClassName = m_windowClass.c_str();
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));
    return RegisterClassExW(&wcex);
}

void MainView::LayoutPlayer()
{
    if (!m_hWnd || !m_playerHwnd || !IsWindow(m_playerHwnd))
    {
        return;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);
    SetWindowPos(
        m_playerHwnd,
        nullptr,
        0,
        0,
        rc.right - rc.left,
        rc.bottom - rc.top,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainView::OnResize()
{
    LayoutPlayer();
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

LRESULT MainView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TIMER:
        if (m_onTimer) m_onTimer(static_cast<UINT_PTR>(wParam));
        return 0;

    case WM_COMMAND:
    {
        const int commandId = LOWORD(wParam);
        if (commandId == IDM_VIEW_FULLSCREEN)
        {
            ToggleFullscreen();
            return 0;
        }
        if (commandId == IDM_EXIT)
        {
            DestroyWindow(m_hWnd);
            return 0;
        }
        if (m_onCommand) m_onCommand(commandId);
        return 0;
    }

    case WM_DROPFILES:
        if (m_onFilesDropped) m_onFilesDropped(reinterpret_cast<HDROP>(wParam));
        else DragFinish(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            ToggleFullscreen();
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            if (m_isFullscreen) ExitFullscreen();
            return 0;
        }
        if (m_onKeyDown) m_onKeyDown(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        if (m_onMouseWheel) m_onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        ToggleFullscreen();
        return 0;

    case WM_ACTIVATE:
        if (m_onActivationChanged) m_onActivationChanged(LOWORD(wParam) != WA_INACTIVE);
        break;

    case WM_ACTIVATEAPP:
        if (m_isFullscreen && m_hWnd)
        {
            if (wParam == TRUE)
            {
                SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
            else if (!(m_savedExStyle & WS_EX_TOPMOST))
            {
                SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
        }
        break;

    case WM_SIZE:
        OnResize();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        if (m_hMenu && GetMenu(m_hWnd) != m_hMenu)
        {
            DestroyMenu(m_hMenu);
        }
        m_hMenu = nullptr;
        PostQuitMessage(0);
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
