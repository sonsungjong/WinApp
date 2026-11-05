#include "Views/MainView.h"
#include "Resource.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <filesystem>

namespace
{
    constexpr int kMaxLoadString = 100;
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 760;
    constexpr int kControlBarHeight = 76;

    constexpr wchar_t kVideoHostClassName[] = L"NextVideoPlayer_VideoHostWnd";
    constexpr wchar_t kControlBarClassName[] = L"NextVideoPlayer_ControlBarWnd";
    constexpr wchar_t kSettingsClassName[] = L"NextVideoPlayer_SettingsWnd";
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

    constexpr int IDC_SET_SEEK_EDIT = 1001;
    constexpr int IDC_SET_REPEAT_CHK = 1002;
    constexpr int IDC_SET_TOPMOST_CHK = 1003;
    constexpr int IDC_SET_SUB_CHK = 1004;
    constexpr int IDC_SET_OK = 1005;
    constexpr int IDC_SET_CANCEL = 1006;

    constexpr int IDC_PL_LIST = 2001;
    constexpr int IDC_PL_PLAY = 2002;
    constexpr int IDC_PL_ADD = 2003;
    constexpr int IDC_PL_REMOVE = 2004;
    constexpr int IDC_PL_CLEAR = 2005;

    std::vector<std::wstring> GetBundledFontPaths()
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(modulePath).parent_path();

        std::vector<std::filesystem::path> baseDirs = {
            exeDir / L"ThirdParty" / L"fonts",
            exeDir.parent_path() / L"ThirdParty" / L"fonts"
        };

        WCHAR localAppData[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0)
        {
            baseDirs.push_back(std::filesystem::path(localAppData) / L"Microsoft" / L"Windows" / L"Fonts");
        }

        std::vector<std::wstring> found;
        for (const wchar_t* fileName : { L"NanumSquareR.ttf", L"NanumSquareB.ttf" })
        {
            for (const auto& dir : baseDirs)
            {
                std::filesystem::path candidate = dir / fileName;
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    found.push_back(candidate.wstring());
                    break;
                }
            }
        }
        return found;
    }

    HFONT GetNanumDialogFont()
    {
        static HFONT s_hFont = nullptr;
        if (!s_hFont)
        {
            s_hFont = CreateFontW(
                -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"NanumSquare");
            if (!s_hFont)
            {
                s_hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            }
        }
        return s_hFont;
    }

    struct SettingsDialogContext
    {
        AppConfig config{};
        bool accepted = false;
        bool done = false;
        HWND hSeekEdit = nullptr;
        HWND hRepeatChk = nullptr;
        HWND hTopMostChk = nullptr;
        HWND hSubChk = nullptr;
    };

    LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* ctx = reinterpret_cast<SettingsDialogContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        switch (msg)
        {
        case WM_NCCREATE:
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            ctx = static_cast<SettingsDialogContext*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
            break;
        }
        case WM_CREATE:
        {
            HFONT hFont = GetNanumDialogFont();

            HWND hLblSeek = CreateWindowExW(0, L"STATIC", L"좌/우 방향키 이동 시간 (초):",
                WS_CHILD | WS_VISIBLE, 20, 22, 180, 20, hWnd, nullptr, nullptr, nullptr);
            SendMessageW(hLblSeek, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

            std::wstring seekStr = std::to_wstring(ctx->config.seekStepSeconds);
            ctx->hSeekEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", seekStr.c_str(),
                WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL, 205, 19, 80, 24,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_SEEK_EDIT)), nullptr, nullptr);
            SendMessageW(ctx->hSeekEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

            ctx->hRepeatChk = CreateWindowExW(0, L"BUTTON", L"반복 재생 사용 (Repeat)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 58, 260, 22,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_REPEAT_CHK)), nullptr, nullptr);
            SendMessageW(ctx->hRepeatChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessageW(ctx->hRepeatChk, BM_SETCHECK, ctx->config.repeatEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

            ctx->hTopMostChk = CreateWindowExW(0, L"BUTTON", L"창 항상 위 고정 (Always on Top)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 88, 260, 22,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_TOPMOST_CHK)), nullptr, nullptr);
            SendMessageW(ctx->hTopMostChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessageW(ctx->hTopMostChk, BM_SETCHECK, ctx->config.topMost ? BST_CHECKED : BST_UNCHECKED, 0);

            ctx->hSubChk = CreateWindowExW(0, L"BUTTON", L"자막 표시 (Subtitle Visible)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 118, 260, 22,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_SUB_CHK)), nullptr, nullptr);
            SendMessageW(ctx->hSubChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            SendMessageW(ctx->hSubChk, BM_SETCHECK, ctx->config.subtitleVisible ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hBtnOk = CreateWindowExW(0, L"BUTTON", L"저장",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 110, 160, 80, 28,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_OK)), nullptr, nullptr);
            SendMessageW(hBtnOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

            HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"취소",
                WS_CHILD | WS_VISIBLE, 205, 160, 80, 28,
                hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_CANCEL)), nullptr, nullptr);
            SendMessageW(hBtnCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            return 0;
        }
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == IDC_SET_OK)
            {
                WCHAR buf[32]{};
                GetWindowTextW(ctx->hSeekEdit, buf, 32);
                int step = _wtoi(buf);
                ctx->config.seekStepSeconds = std::clamp(step > 0 ? step : 5, 1, 600);
                ctx->config.repeatEnabled = (SendMessageW(ctx->hRepeatChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ctx->config.topMost = (SendMessageW(ctx->hTopMostChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ctx->config.subtitleVisible = (SendMessageW(ctx->hSubChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                ctx->accepted = true;
                ctx->done = true;
                DestroyWindow(hWnd);
                return 0;
            }
            if (id == IDC_SET_CANCEL)
            {
                ctx->accepted = false;
                ctx->done = true;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (ctx)
            {
                ctx->accepted = false;
                ctx->done = true;
            }
            DestroyWindow(hWnd);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
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
        WS_CHILD | WS_CLIPSIBLINGS,
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

    m_controlBarHwnd = CreateWindowExW(
        0,
        kControlBarClassName,
        nullptr,
        WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
        0,
        videoH,
        clientW,
        kControlBarHeight,
        m_hWnd,
        nullptr,
        hInstance,
        this);

    if (!m_controlBarHwnd)
    {
        return false;
    }

    SetWindowPos(m_controlBarHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    if (m_hWnd)
    {
        SetWindowTextW(m_hWnd, title.empty() ? m_defaultTitle.c_str() : title.c_str());
    }
}

void MainView::SetStatusText(const std::wstring& statusText)
{
    m_statusText = statusText;
    Render();
}

void MainView::UpdateControlBar(const ControlBarState& state)
{
    m_barState = state;

    if (m_hMenu)
    {
        CheckMenuItem(m_hMenu, IDM_REPEAT_TOGGLE, MF_BYCOMMAND | (state.repeatEnabled ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_hMenu, IDM_SUB_TOGGLE, MF_BYCOMMAND | (state.subtitleVisible ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m_hMenu, IDM_VIEW_TOPMOST, MF_BYCOMMAND | (state.topMost ? MF_CHECKED : MF_UNCHECKED));
    }

    if (m_controlBarHwnd && IsWindow(m_controlBarHwnd))
    {
        InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
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

void MainView::SetTopMost(bool topMost)
{
    if (m_hWnd)
    {
        SetWindowPos(
            m_hWnd,
            topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
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
        m_isControlBarVisible = false;
        m_lastMouseActiveTick = GetTickCount64();
        GetCursorPos(&m_lastMousePt);

        if (m_controlBarHwnd)
        {
            ShowWindow(m_controlBarHwnd, SW_HIDE);
        }

        SetMenu(m_hWnd, nullptr);
        SetWindowLongPtrW(
            m_hWnd,
            GWL_STYLE,
            (m_savedStyle & ~(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE | WS_MINIMIZE)) | WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE);
        SetWindowLongPtrW(
            m_hWnd,
            GWL_EXSTYLE,
            m_savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        SetWindowPos(
            m_hWnd,
            m_barState.topMost ? HWND_TOPMOST : HWND_TOP,
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
        Render();
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
    m_isControlBarVisible = true;

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
        m_barState.topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    UpdateLayout();

    if (m_controlBarHwnd)
    {
        ShowWindow(m_controlBarHwnd, SW_SHOW);
        InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    }

    Render();
}

bool MainView::IsFullscreen() const
{
    return m_isFullscreen;
}

void MainView::UpdateFullscreenAutoHide()
{
    if (!m_hWnd || !m_isFullscreen || !m_controlBarHwnd)
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

    if (inToolbarZone || m_isDraggingSeek || m_isDraggingVolume)
    {
        m_lastMouseActiveTick = GetTickCount64();

        if (!m_isControlBarVisible)
        {
            m_isControlBarVisible = true;
            RECT rc{};
            GetClientRect(m_hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            SetWindowPos(
                m_controlBarHwnd,
                HWND_TOP,
                0,
                h - kControlBarHeight,
                w,
                kControlBarHeight,
                SWP_SHOWWINDOW | SWP_NOACTIVATE);
            InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
        }
    }
    else
    {
        if (m_isControlBarVisible && (GetTickCount64() - m_lastMouseActiveTick >= 200))
        {
            m_isControlBarVisible = false;
            ShowWindow(m_controlBarHwnd, SW_HIDE);
            if (m_isVideoVisible && m_videoHwnd)
            {
                InvalidateRect(m_videoHwnd, nullptr, FALSE);
            }
            else
            {
                Render();
            }
            InvalidateRect(m_hWnd, nullptr, FALSE);
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

bool MainView::ShowSettingsModal(AppConfig& inOutConfig)
{
    SettingsDialogContext ctx{};
    ctx.config = inOutConfig;

    RECT parentRc{};
    GetWindowRect(m_hWnd, &parentRc);
    int dlgW = 320;
    int dlgH = 240;
    int posX = parentRc.left + ((parentRc.right - parentRc.left) - dlgW) / 2;
    int posY = parentRc.top + ((parentRc.bottom - parentRc.top) - dlgH) / 2;

    EnableWindow(m_hWnd, FALSE);

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSettingsClassName,
        L"환경설정 (config.ini)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX,
        posY,
        dlgW,
        dlgH,
        m_hWnd,
        nullptr,
        m_hInstance,
        &ctx);

    MSG msg{};
    while (!ctx.done && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            DestroyWindow(hDlg);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(m_hWnd, TRUE);
    SetForegroundWindow(m_hWnd);

    if (ctx.accepted)
    {
        inOutConfig = ctx.config;
        return true;
    }
    return false;
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
}

void MainView::Render()
{
    if (!m_hWnd)
    {
        return;
    }

    if (FAILED(CreateDeviceResources()))
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

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void MainView::DrawControlBar()
{
    if (!m_controlBarHwnd || !IsWindow(m_controlBarHwnd))
    {
        return;
    }

    if (FAILED(CreateControlBarResources()))
    {
        return;
    }

    m_barRenderTarget->BeginDraw();

    D2D1_SIZE_F rtSize = m_barRenderTarget->GetSize();
    D2D1_RECT_F barRect = D2D1::RectF(0.0f, 0.0f, rtSize.width, rtSize.height);

    // 1. Sleek Dark/Flat Control Bar Background (subtle vertical gradient around 0x1E1E1E)
    if (m_barGradientBrush)
    {
        m_barGradientBrush->SetStartPoint(D2D1::Point2F(0.0f, 0.0f));
        m_barGradientBrush->SetEndPoint(D2D1::Point2F(0.0f, rtSize.height));
        m_barRenderTarget->FillRectangle(barRect, m_barGradientBrush.Get());
    }
    else
    {
        m_barBgBrush->SetColor(D2D1::ColorF(0x1E1E1E));
        m_barRenderTarget->FillRectangle(barRect, m_barBgBrush.Get());
    }

    // Subtle 1px top border separator
    m_barRenderTarget->FillRectangle(D2D1::RectF(0.0f, 0.0f, rtSize.width, 1.0f), m_btnBorderBrush.Get());

    // 2. Thin, Sleek Seek Bar with Hover/Drag Highlight
    m_seekBarRect = D2D1::RectF(16.0f, 4.0f, std::max(32.0f, rtSize.width - 16.0f), 24.0f);
    bool hoverSeek = m_isDraggingSeek ||
        (m_isTrackingBarMouse && PtInRectF(m_seekBarRect, m_hoverMousePt.x, m_hoverMousePt.y));

    float trackCenterY = 14.0f;
    float trackHalfH = hoverSeek ? 3.0f : 1.5f;
    D2D1_ROUNDED_RECT trackRounded = D2D1::RoundedRect(
        D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, m_seekBarRect.right, trackCenterY + trackHalfH),
        trackHalfH,
        trackHalfH);
    m_barRenderTarget->FillRoundedRectangle(trackRounded, m_trackBrush.Get());

    float seekWidth = m_seekBarRect.right - m_seekBarRect.left;

    // Hover preview bar along seek track
    if (hoverSeek && m_hoverMousePt.x >= m_seekBarRect.left)
    {
        float previewX = std::clamp(m_hoverMousePt.x, m_seekBarRect.left, m_seekBarRect.right);
        D2D1_ROUNDED_RECT previewRounded = D2D1::RoundedRect(
            D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, previewX, trackCenterY + trackHalfH),
            trackHalfH,
            trackHalfH);
        m_barRenderTarget->FillRoundedRectangle(previewRounded, m_trackHoverBrush.Get());
    }

    float progressRatio = 0.0f;
    if (m_barState.duration > 0.0)
    {
        progressRatio = static_cast<float>(std::clamp(m_barState.position / m_barState.duration, 0.0, 1.0));
    }

    float filledX = m_seekBarRect.left + seekWidth * progressRatio;
    if (filledX > m_seekBarRect.left)
    {
        D2D1_ROUNDED_RECT filledRounded = D2D1::RoundedRect(
            D2D1::RectF(m_seekBarRect.left, trackCenterY - trackHalfH, filledX, trackCenterY + trackHalfH),
            trackHalfH,
            trackHalfH);
        m_barRenderTarget->FillRoundedRectangle(
            filledRounded,
            hoverSeek ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }

    // A-B markers
    if (m_barState.duration > 0.0)
    {
        if (m_barState.abLoopA >= 0.0)
        {
            float ax = m_seekBarRect.left + seekWidth * static_cast<float>(std::clamp(m_barState.abLoopA / m_barState.duration, 0.0, 1.0));
            m_barRenderTarget->FillRectangle(D2D1::RectF(ax - 1.5f, 7.0f, ax + 1.5f, 21.0f), m_abMarkerBrush.Get());
        }
        if (m_barState.abLoopB >= 0.0)
        {
            float bx = m_seekBarRect.left + seekWidth * static_cast<float>(std::clamp(m_barState.abLoopB / m_barState.duration, 0.0, 1.0));
            m_barRenderTarget->FillRectangle(D2D1::RectF(bx - 1.5f, 7.0f, bx + 1.5f, 21.0f), m_abMarkerBrush.Get());
        }
    }

    // Seek thumb with hover highlight ring
    if (hoverSeek)
    {
        m_accentHoverBrush->SetOpacity(0.28f);
        m_barRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(filledX, trackCenterY), 9.5f, 9.5f), m_accentHoverBrush.Get());
        m_accentHoverBrush->SetOpacity(1.0f);
    }
    float thumbRadius = hoverSeek ? 6.0f : 4.2f;
    m_barRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(filledX, trackCenterY), thumbRadius, thumbRadius), m_barTextBrush.Get());

    // 3. Transport & Control Row (Windows 10/11 Fluent/MDL2 Vector Icons + Segoe UI Timer + NanumSquare Korean UI)
    float rowTop = 30.0f;
    float rowBottom = 64.0f;
    float curX = 14.0f;

    // Play / Pause: \uE768 (▶), \uE769 (⏸)
    bool isPlaying = (m_barState.playbackState == PlaybackState::Playing);
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
    std::wstring posStr = FormatTime(m_barState.position);
    std::wstring durStr = L" / " + FormatTime(m_barState.duration);
    if (m_barState.abLoopA >= 0.0 || m_barState.abLoopB >= 0.0)
    {
        durStr += L"  [A-B]";
    }
    std::wstring fullTimeStr = posStr + durStr;

    D2D1_RECT_F timeRect = D2D1::RectF(curX, rowTop, curX + 220.0f, rowBottom);
    m_barRenderTarget->DrawTextW(
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
    bool playlistOpen = (m_playlistHwnd && IsWindowVisible(m_playlistHwnd));
    m_btnPlaylist = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnPlaylist, L"\uE90B", m_iconTextFormat.Get(), playlistOpen);
    rightX -= 42.0f;

    // Subtitle Toggle (Korean UI text in NanumSquare)
    m_btnSub = D2D1::RectF(rightX - 48.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnSub, L"자막", m_uiTextFormat.Get(), m_barState.subtitleVisible);
    rightX -= 54.0f;

    // Repeat: \uE8EE (🔁) - Default OFF
    m_btnRepeat = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnRepeat, L"\uE8EE", m_iconTextFormat.Get(), m_barState.repeatEnabled);
    rightX -= 46.0f;

    // Speed controls (+, 1.0x, -) in Segoe UI
    m_btnSpeedPlus = D2D1::RectF(rightX - 28.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedPlus, L"+", m_speedTextFormat.Get(), false);
    rightX -= 32.0f;

    WCHAR speedBuf[32]{};
    swprintf_s(speedBuf, L"%.1fx", m_barState.speed);
    m_btnSpeedReset = D2D1::RectF(rightX - 46.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedReset, speedBuf, m_speedTextFormat.Get(), std::abs(m_barState.speed - 1.0) > 0.04);
    rightX -= 50.0f;

    m_btnSpeedMinus = D2D1::RectF(rightX - 28.0f, rowTop + 2.0f, rightX, rowBottom - 2.0f);
    DrawButton(m_btnSpeedMinus, L"-", m_speedTextFormat.Get(), false);
    rightX -= 42.0f;

    // Volume slider with hover highlight
    float volCenterY = (rowTop + rowBottom) * 0.5f;
    m_volSliderRect = D2D1::RectF(rightX - 80.0f, rowTop + 6.0f, rightX, rowBottom - 6.0f);
    bool hoverVol = m_isDraggingVolume ||
        (m_isTrackingBarMouse && PtInRectF(m_volSliderRect, m_hoverMousePt.x, m_hoverMousePt.y));
    float volHalfH = hoverVol ? 2.5f : 1.5f;

    D2D1_ROUNDED_RECT volTrack = D2D1::RoundedRect(
        D2D1::RectF(m_volSliderRect.left, volCenterY - volHalfH, m_volSliderRect.right, volCenterY + volHalfH),
        volHalfH,
        volHalfH);
    m_barRenderTarget->FillRoundedRectangle(volTrack, hoverVol ? m_trackHoverBrush.Get() : m_trackBrush.Get());

    float volRatio = m_barState.muted ? 0.0f : static_cast<float>(std::clamp(m_barState.volume, 0, 100)) / 100.0f;
    float volFillX = m_volSliderRect.left + (m_volSliderRect.right - m_volSliderRect.left) * volRatio;
    if (volFillX > m_volSliderRect.left)
    {
        D2D1_ROUNDED_RECT volFill = D2D1::RoundedRect(
            D2D1::RectF(m_volSliderRect.left, volCenterY - volHalfH, volFillX, volCenterY + volHalfH),
            volHalfH,
            volHalfH);
        m_barRenderTarget->FillRoundedRectangle(volFill, hoverVol ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }

    float volThumbR = hoverVol ? 5.2f : 3.8f;
    m_barRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(volFillX, volCenterY), volThumbR, volThumbR), m_barTextBrush.Get());
    rightX -= 88.0f;

    // Volume / Mute Icon: \uE767 (🔊), \uE74F (🔇)
    bool isMuted = m_barState.muted || (m_barState.volume <= 0);
    m_btnMute = D2D1::RectF(rightX - 36.0f, rowTop, rightX, rowBottom);
    DrawButton(m_btnMute, isMuted ? L"\uE74F" : L"\uE767", m_iconTextFormat.Get(), m_barState.muted);

    HRESULT hr = m_barRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardControlBarResources();
    }
}

void MainView::DrawButton(const D2D1_RECT_F& rect, const std::wstring& label, IDWriteTextFormat* textFormat, bool active)
{
    bool hovered = m_isTrackingBarMouse && PtInRectF(rect, m_hoverMousePt.x, m_hoverMousePt.y);
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect, 6.0f, 6.0f);

    if (active)
    {
        m_barRenderTarget->FillRoundedRectangle(rounded, hovered ? m_accentHoverBrush.Get() : m_accentBrush.Get());
    }
    else if (hovered)
    {
        m_barRenderTarget->FillRoundedRectangle(rounded, m_btnHoverBrush.Get());
        m_barRenderTarget->DrawRoundedRectangle(rounded, m_btnBorderBrush.Get(), 1.0f);
    }
    else
    {
        m_barRenderTarget->FillRoundedRectangle(rounded, m_btnBrush.Get());
    }

    IDWriteTextFormat* fmt = textFormat ? textFormat : m_uiTextFormat.Get();
    m_barRenderTarget->DrawTextW(
        label.c_str(),
        static_cast<UINT32>(label.length()),
        fmt,
        rect,
        (active || hovered) ? m_barTextBrush.Get() : m_barSubTextBrush.Get());
}

std::wstring MainView::FormatTime(double seconds)
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

bool MainView::PtInRectF(const D2D1_RECT_F& rect, float x, float y)
{
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void MainView::SetOnOpenFileRequested(std::function<void()> handler) { m_onOpenFileRequested = std::move(handler); }
void MainView::SetOnOpenSubtitleRequested(std::function<void()> handler) { m_onOpenSubtitleRequested = std::move(handler); }
void MainView::SetOnScreenshotRequested(std::function<void()> handler) { m_onScreenshotRequested = std::move(handler); }
void MainView::SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) { m_onFilesDropped = std::move(handler); }
void MainView::SetOnPlayPauseRequested(std::function<void()> handler) { m_onPlayPauseRequested = std::move(handler); }
void MainView::SetOnStopRequested(std::function<void()> handler) { m_onStopRequested = std::move(handler); }
void MainView::SetOnPrevRequested(std::function<void()> handler) { m_onPrevRequested = std::move(handler); }
void MainView::SetOnNextRequested(std::function<void()> handler) { m_onNextRequested = std::move(handler); }
void MainView::SetOnSeekAbsolute(std::function<void(double)> handler) { m_onSeekAbsolute = std::move(handler); }
void MainView::SetOnSeekDirection(std::function<void(int)> handler) { m_onSeekDirection = std::move(handler); }
void MainView::SetOnVolumeChanged(std::function<void(int)> handler) { m_onVolumeChanged = std::move(handler); }
void MainView::SetOnVolumeStep(std::function<void(int)> handler) { m_onVolumeStep = std::move(handler); }
void MainView::SetOnMuteToggled(std::function<void()> handler) { m_onMuteToggled = std::move(handler); }
void MainView::SetOnSpeedStep(std::function<void(double)> handler) { m_onSpeedStep = std::move(handler); }
void MainView::SetOnSpeedReset(std::function<void()> handler) { m_onSpeedReset = std::move(handler); }
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

    WNDCLASSEXW barWc{};
    barWc.cbSize = sizeof(WNDCLASSEXW);
    barWc.style = CS_HREDRAW | CS_VREDRAW;
    barWc.lpfnWndProc = MainView::ControlBarWndProc;
    barWc.hInstance = hInstance;
    barWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    barWc.hbrBackground = nullptr;
    barWc.lpszClassName = kControlBarClassName;
    RegisterClassExW(&barWc);

    WNDCLASSEXW settingsWc{};
    settingsWc.cbSize = sizeof(WNDCLASSEXW);
    settingsWc.lpfnWndProc = SettingsWndProc;
    settingsWc.hInstance = hInstance;
    settingsWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsWc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    settingsWc.lpszClassName = kSettingsClassName;
    RegisterClassExW(&settingsWc);

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

    // Load bundled NanumSquare fonts for GDI & DirectWrite
    std::vector<std::wstring> fontPaths = GetBundledFontPaths();
    for (const auto& path : fontPaths)
    {
        AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
    }

    Microsoft::WRL::ComPtr<IDWriteFactory5> dwriteFactory5;
    if (!fontPaths.empty() && SUCCEEDED(m_dwriteFactory.As(&dwriteFactory5)))
    {
        Microsoft::WRL::ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
        if (SUCCEEDED(dwriteFactory5->CreateFontSetBuilder(&fontSetBuilder)))
        {
            for (const auto& path : fontPaths)
            {
                Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile;
                if (SUCCEEDED(dwriteFactory5->CreateFontFileReference(path.c_str(), nullptr, &fontFile)))
                {
                    fontSetBuilder->AddFontFile(fontFile.Get());
                }
            }
            Microsoft::WRL::ComPtr<IDWriteFontSet> fontSet;
            if (SUCCEEDED(fontSetBuilder->CreateFontSet(&fontSet)))
            {
                dwriteFactory5->CreateFontCollectionFromFontSet(fontSet.Get(), &m_customFontCollection);
            }
        }
    }

    IDWriteFontCollection* korCollection = m_customFontCollection.Get();
    const wchar_t* korFontName = korCollection ? L"NanumSquare" : L"Malgun Gothic";

    Microsoft::WRL::ComPtr<IDWriteFontCollection> sysCollection;
    m_dwriteFactory->GetSystemFontCollection(&sysCollection);
    if (!korCollection && sysCollection)
    {
        UINT32 idx = 0;
        BOOL exists = FALSE;
        if (SUCCEEDED(sysCollection->FindFamilyName(L"NanumSquare", &idx, &exists)) && exists)
        {
            korFontName = L"NanumSquare";
        }
    }

    // 1. Main status text (NanumSquare Regular 18pt)
    hr = m_dwriteFactory->CreateTextFormat(
        korFontName,
        korCollection,
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

    // 2. Korean UI button text (NanumSquare Bold 12.5pt)
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

    // 3. Windows 10/11 Built-in Icon Font (Segoe Fluent Icons with fallback to Segoe MDL2 Assets)
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

    // 4. Timer text (Segoe UI SemiBold 12.5pt, left-aligned)
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

    // 5. Speed text (Segoe UI SemiBold 12.0pt, centered)
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

HRESULT MainView::CreateDeviceResources()
{
    if (m_renderTarget)
    {
        return S_OK;
    }

    RECT rc{};
    GetClientRect(m_hWnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hWnd, size),
        m_renderTarget.GetAddressOf());
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

HRESULT MainView::CreateControlBarResources()
{
    if (m_barRenderTarget)
    {
        return S_OK;
    }

    if (!m_controlBarHwnd || !IsWindow(m_controlBarHwnd))
    {
        return E_FAIL;
    }

    RECT rc{};
    GetClientRect(m_controlBarHwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(std::max(1L, rc.right - rc.left), std::max(1L, rc.bottom - rc.top));
    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_controlBarHwnd, size),
        m_barRenderTarget.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x232327);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x18181B);

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
    if (SUCCEEDED(m_barRenderTarget->CreateGradientStopCollection(stops, 2, &stopCollection)))
    {
        m_barRenderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0f, 0.0f),
                D2D1::Point2F(0.0f, static_cast<float>(kControlBarHeight))),
            stopCollection.Get(),
            m_barGradientBrush.GetAddressOf());
    }

    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF5F5F7), m_barTextBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xD4D4DC), m_barSubTextBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1E1E1E), m_barBgBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x25252A), m_btnBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x32323A), m_btnHoverBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x3D3D47), m_btnBorderBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2478E8), m_accentBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x3B8EFF), m_accentHoverBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x383842), m_trackBrush.GetAddressOf());
    m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x565664), m_trackHoverBrush.GetAddressOf());
    return m_barRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFBE26), m_abMarkerBrush.GetAddressOf());
}

void MainView::DiscardControlBarResources()
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
    m_barRenderTarget.Reset();
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
        if (m_controlBarHwnd)
        {
            SetWindowPos(
                m_controlBarHwnd,
                HWND_TOP,
                0,
                h - kControlBarHeight,
                w,
                kControlBarHeight,
                (m_isControlBarVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) | SWP_NOACTIVATE);
        }

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
        if (m_controlBarHwnd)
        {
            SetWindowPos(
                m_controlBarHwnd,
                HWND_TOP,
                0,
                videoH,
                w,
                kControlBarHeight,
                SWP_SHOWWINDOW | SWP_NOACTIVATE);
        }

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

    if (m_renderTarget)
    {
        m_renderTarget->Resize(D2D1::SizeU(w, h));
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

LRESULT CALLBACK MainView::ControlBarWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        return view->HandleControlBarMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT MainView::HandleControlBarMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hWnd, &ps);
        DrawControlBar();
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        if (m_barRenderTarget)
        {
            m_barRenderTarget->Resize(D2D1::SizeU(width, height));
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        OnControlBarLButtonDown(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
        return 0;
    case WM_MOUSEMOVE:
        OnControlBarMouseMove(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
        return 0;
    case WM_MOUSELEAVE:
        OnControlBarMouseLeave();
        return 0;
    case WM_LBUTTONUP:
        OnControlBarLButtonUp(static_cast<float>(GET_X_LPARAM(lParam)), static_cast<float>(GET_Y_LPARAM(lParam)));
        return 0;
    case WM_KEYDOWN:
        // Forward shortcut keys to main window
        SendMessageW(m_hWnd, WM_KEYDOWN, wParam, lParam);
        return 0;
    case WM_DESTROY:
        DiscardControlBarResources();
        return 0;
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
        HFONT hFont = GetNanumDialogFont();
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
        if (m_controlBarHwnd)
        {
            InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
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

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(m_hWnd, &ps);
        Render();
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

void MainView::OnControlBarLButtonDown(float x, float y)
{
    SetFocus(m_hWnd);

    if (PtInRectF(m_seekBarRect, x, y))
    {
        m_isDraggingSeek = true;
        SetCapture(m_controlBarHwnd);
        SeekFromMouseX(x);
        return;
    }

    if (PtInRectF(m_volSliderRect, x, y))
    {
        m_isDraggingVolume = true;
        SetCapture(m_controlBarHwnd);
        VolumeFromMouseX(x);
        return;
    }

    if (PtInRectF(m_btnPlayPause, x, y) && m_onPlayPauseRequested) m_onPlayPauseRequested();
    else if (PtInRectF(m_btnStop, x, y) && m_onStopRequested) m_onStopRequested();
    else if (PtInRectF(m_btnPrev, x, y) && m_onPrevRequested) m_onPrevRequested();
    else if (PtInRectF(m_btnNext, x, y) && m_onNextRequested) m_onNextRequested();
    else if (PtInRectF(m_btnMute, x, y) && m_onMuteToggled) m_onMuteToggled();
    else if (PtInRectF(m_btnSpeedMinus, x, y) && m_onSpeedStep) m_onSpeedStep(-0.1);
    else if (PtInRectF(m_btnSpeedReset, x, y) && m_onSpeedReset) m_onSpeedReset();
    else if (PtInRectF(m_btnSpeedPlus, x, y) && m_onSpeedStep) m_onSpeedStep(0.1);
    else if (PtInRectF(m_btnRepeat, x, y) && m_onRepeatToggled) m_onRepeatToggled();
    else if (PtInRectF(m_btnSub, x, y) && m_onSubtitleToggle) m_onSubtitleToggle();
    else if (PtInRectF(m_btnPlaylist, x, y)) TogglePlaylistWindow();
    else if (PtInRectF(m_btnSettings, x, y) && m_onSettingsRequested) m_onSettingsRequested();
}

void MainView::OnControlBarMouseMove(float x, float y)
{
    m_hoverMousePt = D2D1::Point2F(x, y);

    if (!m_isTrackingBarMouse && m_controlBarHwnd)
    {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_controlBarHwnd, 0 };
        if (TrackMouseEvent(&tme))
        {
            m_isTrackingBarMouse = true;
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

    if (m_controlBarHwnd)
    {
        InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    }
}

void MainView::OnControlBarMouseLeave()
{
    m_isTrackingBarMouse = false;
    m_hoverMousePt = D2D1::Point2F(-1.0f, -1.0f);
    if (m_controlBarHwnd)
    {
        InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    }
}

void MainView::OnControlBarLButtonUp(float, float)
{
    if (m_isDraggingSeek || m_isDraggingVolume)
    {
        m_isDraggingSeek = false;
        m_isDraggingVolume = false;
        ReleaseCapture();
        if (m_controlBarHwnd)
        {
            InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
        }
    }
}

void MainView::SeekFromMouseX(float x)
{
    if (m_barState.duration <= 0.0 || !m_onSeekAbsolute)
    {
        return;
    }

    float width = std::max(1.0f, m_seekBarRect.right - m_seekBarRect.left);
    float ratio = std::clamp((x - m_seekBarRect.left) / width, 0.0f, 1.0f);
    m_onSeekAbsolute(static_cast<double>(ratio) * m_barState.duration);
}

void MainView::VolumeFromMouseX(float x)
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
