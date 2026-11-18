#include "Views/SettingsView.h"
#include "Views/FontHelper.h"
#include "Resource.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <filesystem>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace
{
    constexpr wchar_t kSettingsClassName[] = L"NextVideoPlayer_SettingsWnd";
    constexpr int kDlgWidth = 620;
    constexpr int kDlgHeight = 484;
    constexpr float kHeaderHeight = 56.0f;
    constexpr float kTabBarTop = 64.0f;
    constexpr float kTabBarBottom = 100.0f;
    constexpr float kFooterTop = 422.0f;
    constexpr UINT_PTR kTimerRefreshId = 2;
    constexpr int kIdApiCopyGuide = 609;
    constexpr int kIdScrollArea = 950;

    // Header & Tab IDs
    constexpr int kIdCloseBtn = 50;
    constexpr int kIdTabFile = 100;
    constexpr int kIdTabPlayback = 101;
    constexpr int kIdTabSubtitle = 102;
    constexpr int kIdTabView = 103;
    constexpr int kIdTabApi = 104;
    constexpr int kIdTabInfo = 105;
    constexpr int kTabCount = 6;
    constexpr int kIdApiEnable = 601;
    constexpr int kIdApiIp = 602;
    constexpr int kIdApiPort = 603;
    constexpr int kIdApiPage = 604;
    constexpr int kIdApiCopy = 605;
    constexpr int kIdApiGuide = 606;
    constexpr int kIdApiGuideText = 607;
    constexpr int kIdApiOpenReadme = 608;
    constexpr int kIdInfoText = 701;
    constexpr int kIdInfoEmail = 702;
    constexpr int kIdInfoYoutube = 703;
    constexpr int kIdInfoLicense = 704;

    // Action & Setting Control IDs
    constexpr int kIdFileOpen = 201;
    constexpr int kIdSubOpen = 202;
    constexpr int kIdScreenshot = 203;
    constexpr int kIdExitApp = 204;

    constexpr int kIdSeekMinus = 301;
    constexpr int kIdSeekDisplay = 302;
    constexpr int kIdSeekPlus = 303;
    constexpr int kIdToggleRepeat = 304;
    constexpr int kIdABSetA = 305;
    constexpr int kIdABSetB = 306;
    constexpr int kIdABClear = 307;
    constexpr int kIdPrevFile = 308;
    constexpr int kIdPlayPause = 309;
    constexpr int kIdStop = 310;
    constexpr int kIdNextFile = 311;

    constexpr int kIdToggleSub = 401;
    constexpr int kIdSubScaleMinus = 402;
    constexpr int kIdSubScaleDisplay = 403;
    constexpr int kIdSubScalePlus = 404;
    constexpr int kIdSubDelayMinus = 405;
    constexpr int kIdSubDelayPlus = 406;
    constexpr int kIdSubPosUp = 407;
    constexpr int kIdSubPosDown = 408;

    constexpr int kIdToggleTopMost = 501;
    constexpr int kIdFullscreen = 502;
    constexpr int kIdPlaylist = 503;

    // Footer Button IDs
    constexpr int kIdBtnSave = 900;
    constexpr int kIdBtnCancel = 901;

    constexpr wchar_t kApiGuideFullText[] =
        L"NextVideoPlayer WebSocket API 가이드\r\n"
        L"========================================\r\n\r\n"
        L"1. 바로 사용하기\r\n"
        L"- [API] 탭에서 토글을 켜고 [저장]을 누르거나 [켜고 테스트창 열기]를 클릭합니다.\r\n"
        L"- 기본 WebSocket 주소: ws://127.0.0.1:49152/api/v1\r\n"
        L"- 기본 웹 제어화면: http://127.0.0.1:49152/\r\n"
        L"- 다른 PC에서 접속하려면 수신 IP를 해당 PC IP(또는 0.0.0.0)로 바꾸고 방화벽 포트를 허용하세요.\r\n\r\n"
        L"2. 메시지 형식 (UTF-8 JSON, 필수 \"type\":\"all\")\r\n"
        L"- 제어 명령 예시:\r\n"
        L"  {\"type\":\"all\",\"requestId\":\"pause-1\",\"playback\":\"paused\"}\r\n"
        L"  {\"type\":\"all\",\"speed\":1.5,\"volume\":30,\"muted\":false}\r\n"
        L"  {\"type\":\"all\",\"files\":[\"C:\\\\Videos\\\\a.mp4\"],\"playback\":\"playing\",\"positionMs\":0}\r\n"
        L"- 상태 조회 예시 (알림 끄고 1회 조회):\r\n"
        L"  {\"type\":\"all\",\"requestId\":\"q1\",\"subscribe\":false}\r\n\r\n"
        L"3. 지원 키 목록 (생략하거나 null/\"\" 전달 시 현재 값 유지)\r\n"
        L"- type: 필수 \"all\"\r\n"
        L"- requestId: 선택 문자열(최대 128B), 응답에 그대로 반환\r\n"
        L"- files: 로컬 영상 절대경로 배열(최대 512개), 추가 후 첫 항목 재생\r\n"
        L"- playlistIndex: 0부터 시작하는 재생목록 인덱스\r\n"
        L"- playback: \"playing\" | \"paused\" | \"stopped\"\r\n"
        L"- positionMs: 0 이상 재생 위치(ms)\r\n"
        L"- speed: 0.20 ~ 16.00 (실수 배속)\r\n"
        L"- volume: 0 ~ 100 (정수)\r\n"
        L"- muted / repeat / fullscreen / topMost: true | false\r\n"
        L"- subscribe: 상태 브로드캐스트(최대 ~30Hz) 수신 여부 (기본 true)\r\n"
        L"- executeAtMs: Unix epoch(ms), 최대 30초 후까지 예약 적용\r\n\r\n"
        L"4. 설정 파일(config.ini) 및 다중 인스턴스\r\n"
        L"- 일반/MSI 설치: %LOCALAPPDATA%\\NextVideoPlayer\\config.ini\r\n"
        L"- MSIX 설치: 패키지의 LocalState\\config.ini\r\n"
        L"- 여러 창 독립 제어: NextVideoPlayer.exe --config C:\\Path\\screen2.ini\r\n\r\n"
        L"5. 동봉된 PowerShell 테스트 스크립트 (<설치폴더>\\API\\Send-PlayerCommand.ps1)\r\n"
        L"  powershell -ExecutionPolicy Bypass -File .\\API\\Send-PlayerCommand.ps1\r\n"
        L"  powershell -ExecutionPolicy Bypass -File .\\API\\Send-PlayerCommand.ps1 -Message '{\"type\":\"all\",\"playback\":\"paused\"}'";

    D2D1_RECT_F GetTabRect(int tabIndex)
    {
        const float x = 24.0f + static_cast<float>(tabIndex) * 96.0f;
        return D2D1::RectF(x, kTabBarTop + 4.0f, x + 92.0f, kTabBarBottom - 4.0f);
    }

    D2D1_RECT_F GetCloseButtonRect()
    {
        return D2D1::RectF(static_cast<float>(kDlgWidth) - 46.0f, 14.0f,
                           static_cast<float>(kDlgWidth) - 18.0f, 42.0f);
    }

    D2D1_RECT_F GetCancelButtonRect()
    {
        return D2D1::RectF(404.0f, 436.0f, 496.0f, 470.0f);
    }

    D2D1_RECT_F GetSaveButtonRect()
    {
        return D2D1::RectF(508.0f, 436.0f, 600.0f, 470.0f);
    }

    bool PtInRectF(const D2D1_RECT_F& rc, float x, float y)
    {
        return x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom;
    }

    HRESULT MakeSolidBrush(
        ID2D1RenderTarget* rt,
        UINT32 rgb,
        float alpha,
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>& outBrush)
    {
        return rt->CreateSolidColorBrush(D2D1::ColorF(rgb, alpha), outBrush.ReleaseAndGetAddressOf());
    }
}

void SettingsView::Brushes::Reset()
{
    owner = nullptr;
    gradHeight = 0.0f;
    bg.Reset();
    windowBorder.Reset();
    divider.Reset();
    tabTrackBg.Reset();
    tabTrackBorder.Reset();
    activeTabBg.Reset();
    activeTabBorder.Reset();
    hoverTabBg.Reset();
    cardBg.Reset();
    cardBorder.Reset();
    insetBg.Reset();
    insetBorder.Reset();
    text.Reset();
    subText.Reset();
    mutedText.Reset();
    codeText.Reset();
    darkText.Reset();
    cyan.Reset();
    cyanGlow.Reset();
    toggleOffBg.Reset();
    toggleOffBorder.Reset();
    toggleOffKnob.Reset();
    whiteKnob.Reset();
    btnBg.Reset();
    btnHover.Reset();
    btnPressed.Reset();
    btnBorder.Reset();
    accentBtnBg.Reset();
    accentBtnHover.Reset();
    closeHover.Reset();
    scrollTrack.Reset();
    scrollThumb.Reset();
    saveGrad.Reset();
}

HRESULT SettingsView::Brushes::Ensure(ID2D1RenderTarget* rt)
{
    if (!rt) return E_INVALIDARG;
    if (owner == rt && bg && saveGrad) return S_OK;

    Reset();
    HRESULT hr = S_OK;
    if (FAILED(hr = MakeSolidBrush(rt, 0x161922, 1.0f, bg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x2E3548, 1.0f, windowBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x23293A, 1.0f, divider)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x10131B, 1.0f, tabTrackBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x222839, 1.0f, tabTrackBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x1E2434, 1.0f, activeTabBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x313C56, 1.0f, activeTabBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x171C28, 1.0f, hoverTabBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x1E222E, 1.0f, cardBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x2B3142, 1.0f, cardBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x121620, 1.0f, insetBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x2A3246, 1.0f, insetBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0xF3F6FB, 1.0f, text)) ||
        FAILED(hr = MakeSolidBrush(rt, 0xA8B2C6, 1.0f, subText)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x768097, 1.0f, mutedText)) ||
        FAILED(hr = MakeSolidBrush(rt, 0xD2DAEA, 1.0f, codeText)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x061826, 1.0f, darkText)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x00D8FF, 1.0f, cyan)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x00C8FF, 0.24f, cyanGlow)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x131722, 1.0f, toggleOffBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x363F56, 1.0f, toggleOffBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x8D97AD, 1.0f, toggleOffKnob)) ||
        FAILED(hr = MakeSolidBrush(rt, 0xFFFFFF, 1.0f, whiteKnob)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x161B27, 1.0f, btnBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x232B3E, 1.0f, btnHover)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x11151F, 1.0f, btnPressed)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x2E374C, 1.0f, btnBorder)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x122638, 1.0f, accentBtnBg)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x18344E, 1.0f, accentBtnHover)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x283044, 1.0f, closeHover)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x1B2130, 1.0f, scrollTrack)) ||
        FAILED(hr = MakeSolidBrush(rt, 0x3D4761, 1.0f, scrollThumb)))
    {
        Reset();
        return hr;
    }

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x00E1FF, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x0EA5E9, 1.0f);

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
    hr = rt->CreateGradientStopCollection(stops, 2, stopCollection.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        const D2D1_RECT_F rcSave = GetSaveButtonRect();
        hr = rt->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(rcSave.left, rcSave.top),
                D2D1::Point2F(rcSave.right, rcSave.bottom)),
            stopCollection.Get(),
            saveGrad.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr))
    {
        Reset();
        return hr;
    }

    owner = rt;
    return S_OK;
}

SettingsView::SettingsView(const AppInfo& appInfo, HFONT dialogFont)
    : m_dialogFont(dialogFont), m_appInfo(appInfo)
{
    m_editBackground = CreateSolidBrush(RGB(18, 22, 32));
}

SettingsView::~SettingsView()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        DestroyWindow(m_hWnd);
    }
    DestroyOwnedFontsAndBrushes();
}

void SettingsView::DestroyOwnedFontsAndBrushes()
{
    if (m_ownedEditFont)
    {
        DeleteObject(m_ownedEditFont);
        m_ownedEditFont = nullptr;
    }
    if (m_editBackground)
    {
        DeleteObject(m_editBackground);
        m_editBackground = nullptr;
    }
    DiscardDeviceResources();
}

bool SettingsView::ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig)
{
    m_hInstance = hInstance;
    m_parentHwnd = parentHwnd;
    m_currentConfig = initialConfig;
    m_activeTab = 1;
    m_showingApiGuide = false;
    m_showingLicenses = false;
    m_apiSummaryScroll = 0.0f;
    m_apiGuideScroll = 0.0f;
    m_infoScroll = 0.0f;
    m_isDraggingScroll = false;
    m_footerFeedback.clear();
    m_feedbackExpireTick = 0;
    m_running = false;
    m_accepted = false;
    m_hoveredId = 0;
    m_pressedId = 0;
    m_pendingPostCommand = 0;
    m_ipFocused = false;
    m_portFocused = false;

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    const bool parentWasEnabled = parentHwnd && IsWindow(parentHwnd) && IsWindowEnabled(parentHwnd);
    if (parentWasEnabled)
    {
        EnableWindow(parentHwnd, FALSE);
    }

    if (!CreateDialogWindow())
    {
        if (parentWasEnabled)
        {
            EnableWindow(parentHwnd, TRUE);
        }
        return false;
    }

    if (!CreateApiEditors())
    {
        DestroyWindow(m_hWnd);
        if (parentWasEnabled)
        {
            EnableWindow(parentHwnd, TRUE);
        }
        return false;
    }

    RefreshEditFontForDpi(m_currentDpi);
    LayoutApiEditors();
    UpdateTabVisibility();
    SetTimer(m_hWnd, kTimerRefreshId, 250, nullptr);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hWnd, 20, &darkMode, sizeof(darkMode));
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hWnd, 33, &cornerPref, sizeof(cornerPref));
    COLORREF borderCol = RGB(0x2E, 0x35, 0x48);
    DwmSetWindowAttribute(m_hWnd, 34, &borderCol, sizeof(borderCol));
    SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    m_running = true;

    MSG msg{};
    while (m_running && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            CloseModal(false);
            continue;
        }
        if (IsWindow(m_hWnd) && IsDialogMessageW(m_hWnd, &msg))
        {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (parentWasEnabled && IsWindow(parentHwnd))
    {
        EnableWindow(parentHwnd, TRUE);
        SetForegroundWindow(parentHwnd);
    }

    if (m_hWnd && IsWindow(m_hWnd))
    {
        KillTimer(m_hWnd, kTimerRefreshId);
        DestroyWindow(m_hWnd);
    }

    m_hWnd = nullptr;
    m_ipEdit = nullptr;
    m_portEdit = nullptr;

    if (m_pendingPostCommand != 0 && parentHwnd && IsWindow(parentHwnd))
    {
        PostMessageW(parentHwnd, WM_COMMAND, MAKEWPARAM(m_pendingPostCommand, 0), 0);
        m_pendingPostCommand = 0;
    }

    return m_accepted;
}

bool SettingsView::CreateDialogWindow()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    if (!GetClassInfoExW(m_hInstance, kSettingsClassName, &wc))
    {
        wc.lpfnWndProc = &SettingsView::WindowProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));
        wc.lpszClassName = kSettingsClassName;
        if (!RegisterClassExW(&wc))
        {
            return false;
        }
    }

    m_currentDpi = FontHelper::GetWindowDpi(m_parentHwnd);
    const int physW = FontHelper::ScaleDipToPx(kDlgWidth, m_currentDpi);
    const int physH = FontHelper::ScaleDipToPx(kDlgHeight, m_currentDpi);

    HWND hDlg = CreateWindowExW(
        0,
        kSettingsClassName,
        L"환경설정",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, physW, physH,
        m_parentHwnd,
        nullptr,
        m_hInstance,
        this);
    if (!hDlg)
    {
        return false;
    }

    CenterToParent(m_parentHwnd, m_currentDpi);
    return true;
}

void SettingsView::CenterToParent(HWND parentHwnd, UINT dpi) const
{
    if (!m_hWnd) return;

    const int w = FontHelper::ScaleDipToPx(kDlgWidth, dpi);
    const int h = FontHelper::ScaleDipToPx(kDlgHeight, dpi);

    RECT area{};
    if (parentHwnd && IsWindow(parentHwnd))
    {
        GetWindowRect(parentHwnd, &area);
    }
    else
    {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    }

    int x = area.left + ((area.right - area.left) - w) / 2;
    int y = area.top + ((area.bottom - area.top) - h) / 2;

    HMONITOR mon = MonitorFromRect(&area, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(mon, &mi))
    {
        x = std::clamp(x, static_cast<int>(mi.rcWork.left), (std::max)(static_cast<int>(mi.rcWork.left), static_cast<int>(mi.rcWork.right) - w));
        y = std::clamp(y, static_cast<int>(mi.rcWork.top), (std::max)(static_cast<int>(mi.rcWork.top), static_cast<int>(mi.rcWork.bottom) - h));
    }

    const bool parentTopMost = parentHwnd && ((GetWindowLongPtrW(parentHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
    SetWindowPos(m_hWnd, parentTopMost ? HWND_TOPMOST : HWND_TOP, x, y, w, h, SWP_NOACTIVATE);
}

bool SettingsView::CreateApiEditors()
{
    m_ipEdit = CreateWindowExW(
        0, L"EDIT", m_currentConfig.api.ip.c_str(),
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 100, 20, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdApiIp)), m_hInstance, nullptr);

    m_portEdit = CreateWindowExW(
        0, L"EDIT", std::to_wstring(m_currentConfig.api.port).c_str(),
        WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
        0, 0, 80, 20, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdApiPort)), m_hInstance, nullptr);

    if (!m_ipEdit || !m_portEdit)
    {
        return false;
    }

    SendMessageW(m_ipEdit, EM_SETLIMITTEXT, 100, 0);
    SendMessageW(m_portEdit, EM_SETLIMITTEXT, 5, 0);
    return true;
}

void SettingsView::RefreshEditFontForDpi(UINT dpi)
{
    HFONT newFont = FontHelper::CreateGdiFont(dpi, -14);
    HFONT applyFont = newFont ? newFont : (m_dialogFont ? m_dialogFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));

    for (HWND edit : { m_ipEdit, m_portEdit })
    {
        if (edit && IsWindow(edit))
        {
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(applyFont), TRUE);
            SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));
        }
    }

    if (m_ownedEditFont)
    {
        DeleteObject(m_ownedEditFont);
    }
    m_ownedEditFont = newFont;
}

void SettingsView::LayoutApiEditors()
{
    if (!m_hWnd) return;
    const UINT dpi = FontHelper::GetWindowDpi(m_hWnd);
    m_currentDpi = dpi;

    if (m_ipEdit && IsWindow(m_ipEdit))
    {
        const int x = FontHelper::ScaleDipToPx(122, dpi);
        const int y = FontHelper::ScaleDipToPx(183, dpi);
        const int w = FontHelper::ScaleDipToPx(188, dpi);
        const int h = FontHelper::ScaleDipToPx(18, dpi);
        SetWindowPos(m_ipEdit, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (m_portEdit && IsWindow(m_portEdit))
    {
        const int x = FontHelper::ScaleDipToPx(122, dpi);
        const int y = FontHelper::ScaleDipToPx(217, dpi);
        const int w = FontHelper::ScaleDipToPx(88, dpi);
        const int h = FontHelper::ScaleDipToPx(18, dpi);
        SetWindowPos(m_portEdit, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void SettingsView::UpdateTabVisibility()
{
    const bool showApiEditors = (m_activeTab == 4 && !m_showingApiGuide);
    for (HWND edit : { m_ipEdit, m_portEdit })
    {
        if (edit && IsWindow(edit))
        {
            ShowWindow(edit, showApiEditors ? SW_SHOW : SW_HIDE);
        }
    }
    if (!showApiEditors)
    {
        m_ipFocused = false;
        m_portFocused = false;
    }
}

void SettingsView::ToggleApiGuide()
{
    m_showingApiGuide = !m_showingApiGuide;
    m_apiGuideScroll = 0.0f;
    m_isDraggingScroll = false;
    UpdateTabVisibility();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void SettingsView::OpenApiReadme()
{
    std::vector<wchar_t> exePath(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    while (len == exePath.size() && exePath.size() < 32768)
    {
        exePath.resize(exePath.size() * 2, L'\0');
        len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    }

    if (len > 0 && len < exePath.size())
    {
        std::error_code ec;
        const auto exeDir = std::filesystem::path(std::wstring(exePath.data(), len)).parent_path();
        for (const auto& candidate : { exeDir / L"API" / L"README.md", exeDir.parent_path() / L"API" / L"README.md" })
        {
            if (std::filesystem::exists(candidate, ec))
            {
                const auto res = reinterpret_cast<INT_PTR>(
                    ShellExecuteW(m_hWnd, L"open", candidate.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
                if (res > 32)
                {
                    return;
                }
            }
        }
    }

    if (!m_showingApiGuide)
    {
        ToggleApiGuide();
    }
    MessageBoxW(m_hWnd,
                L"설치 폴더의 API\\README.md 파일을 직접 열 수 없어 앱 내 상세 가이드를 표시했습니다.",
                L"API 가이드", MB_OK | MB_ICONINFORMATION);
}

void SettingsView::ToggleLicenses()
{
    m_showingLicenses = !m_showingLicenses;
    m_infoScroll = 0.0f;
    m_isDraggingScroll = false;
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void SettingsView::OpenInfoLink(const std::wstring& target)
{
    const auto result =
        reinterpret_cast<INT_PTR>(ShellExecuteW(m_hWnd, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
    {
        MessageBoxW(m_hWnd, L"연결된 프로그램을 열 수 없습니다. 정보 화면의 주소를 복사해 사용하세요.",
                    L"앱 정보", MB_OK | MB_ICONINFORMATION);
    }
}

void SettingsView::CopyTextWithFeedback(const std::wstring& text, const std::wstring& statusMessage)
{
    if (!OpenClipboard(m_hWnd))
    {
        return;
    }
    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem)
    {
        void* ptr = GlobalLock(mem);
        if (ptr)
        {
            std::memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(mem);
            SetClipboardData(CF_UNICODETEXT, mem);
        }
        else
        {
            GlobalFree(mem);
        }
    }
    CloseClipboard();

    m_footerFeedback = statusMessage;
    m_feedbackExpireTick = GetTickCount64() + 2600;
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void SettingsView::CopyApiUrl()
{
    AppConfig config{};
    if (!TryReadFormValues(m_currentConfig, config))
    {
        return;
    }
    const auto text = L"ws" + config.api.HttpUrl().substr(4) + L"api/v1";
    CopyTextWithFeedback(text, L"WebSocket 주소가 클립보드에 복사되었습니다: " + text);
}

AppConfig SettingsView::ReadFormValues(const AppConfig&) const
{
    return m_currentConfig;
}

bool SettingsView::TryReadFormValues(const AppConfig&, AppConfig& result)
{
    auto config = m_currentConfig;
    wchar_t ip[128]{};
    wchar_t port[16]{};
    if (m_ipEdit && IsWindow(m_ipEdit))
    {
        GetWindowTextW(m_ipEdit, ip, 128);
        config.api.ip = ip;
    }
    if (m_portEdit && IsWindow(m_portEdit))
    {
        GetWindowTextW(m_portEdit, port, 16);
    }

    if (!ApiSettings::IsValidIp(config.api.ip))
    {
        m_activeTab = 4;
        m_showingApiGuide = false;
        UpdateTabVisibility();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        MessageBoxW(m_hWnd, L"올바른 IP 주소를 입력하세요(127.0.0.1 등).", L"API 설정", MB_OK | MB_ICONINFORMATION);
        if (m_ipEdit) SetFocus(m_ipEdit);
        return false;
    }

    wchar_t* endPtr = nullptr;
    const long parsed = wcstol(port, &endPtr, 10);
    if (port[0] == L'\0' || (endPtr && *endPtr != L'\0') || parsed < 1 || parsed > 65535)
    {
        m_activeTab = 4;
        m_showingApiGuide = false;
        UpdateTabVisibility();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        MessageBoxW(m_hWnd, L"포트는 1~65535 사이의 숫자로 입력하세요.", L"API 설정", MB_OK | MB_ICONINFORMATION);
        if (m_portEdit) SetFocus(m_portEdit);
        return false;
    }

    config.api.port = static_cast<int>(parsed);
    result = config;
    m_currentConfig = config;
    return true;
}

void SettingsView::CloseModal(bool accepted)
{
    m_accepted = accepted;
    m_running = false;
    PostMessageW(m_hWnd, WM_NULL, 0, 0);
}

void SettingsView::SetOnSaveClicked(std::function<void()> handler)
{
    m_onSaveClicked = std::move(handler);
}

void SettingsView::SetOnCancelClicked(std::function<void()> handler)
{
    m_onCancelClicked = std::move(handler);
}

void SettingsView::SetPreviewStateForTest(int activeTab, bool showingApiGuide, bool showingLicenses)
{
    m_activeTab = std::clamp(activeTab, 0, kTabCount - 1);
    m_showingApiGuide = showingApiGuide;
    m_showingLicenses = showingLicenses;
}

HRESULT SettingsView::CreateDeviceIndependentResources()
{
    if (!m_d2dFactory)
    {
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
        if (FAILED(hr)) return hr;
    }

    if (!m_dwriteFactory)
    {
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
        if (FAILED(hr)) return hr;
    }

    if (m_titleFormat && m_scrollTextFormat)
    {
        return S_OK;
    }

    const wchar_t* korFontName = L"NanumSquare";
    FontHelper::CreateCustomFontCollection(m_dwriteFactory.Get(), m_customFontCollection, korFontName);

    auto makeFormat = [&](float size, DWRITE_FONT_WEIGHT weight, DWRITE_TEXT_ALIGNMENT hAlign, DWRITE_PARAGRAPH_ALIGNMENT vAlign, DWRITE_WORD_WRAPPING wrap, Microsoft::WRL::ComPtr<IDWriteTextFormat>& outFmt) -> HRESULT
    {
        HRESULT hr = m_dwriteFactory->CreateTextFormat(
            korFontName,
            m_customFontCollection.Get(),
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"ko-kr",
            outFmt.ReleaseAndGetAddressOf());
        if (SUCCEEDED(hr))
        {
            outFmt->SetTextAlignment(hAlign);
            outFmt->SetParagraphAlignment(vAlign);
            outFmt->SetWordWrapping(wrap);
        }
        return hr;
    };

    HRESULT hr = S_OK;
    if (FAILED(hr = makeFormat(16.5f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_titleFormat)) ||
        FAILED(hr = makeFormat(12.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_subTitleFormat)) ||
        FAILED(hr = makeFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_tabFormat)) ||
        FAILED(hr = makeFormat(13.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_tabBoldFormat)) ||
        FAILED(hr = makeFormat(12.5f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_cardHeaderFormat)) ||
        FAILED(hr = makeFormat(11.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_cardSubTitleFormat)) ||
        FAILED(hr = makeFormat(13.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_labelFormat)) ||
        FAILED(hr = makeFormat(13.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_labelBoldFormat)) ||
        FAILED(hr = makeFormat(12.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_btnFormat)) ||
        FAILED(hr = makeFormat(13.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_NO_WRAP, m_btnBoldFormat)) ||
        FAILED(hr = makeFormat(11.5f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_WRAP, m_captionFormat)) ||
        FAILED(hr = makeFormat(12.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_WORD_WRAPPING_WRAP, m_scrollTextFormat)))
    {
        return hr;
    }

    return S_OK;
}

HRESULT SettingsView::CreateDeviceResources()
{
    if (FAILED(CreateDeviceIndependentResources()))
    {
        return E_FAIL;
    }

    if (!m_renderTarget)
    {
        RECT rc{};
        GetClientRect(m_hWnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(
            (std::max)(1L, rc.right - rc.left),
            (std::max)(1L, rc.bottom - rc.top));

        HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hWnd, size),
            m_renderTarget.GetAddressOf());
        if (FAILED(hr))
        {
            return hr;
        }

        const float dpi = static_cast<float>(FontHelper::GetWindowDpi(m_hWnd));
        m_renderTarget->SetDpi(dpi, dpi);
        m_renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }

    return m_brushes.Ensure(m_renderTarget.Get());
}

void SettingsView::DiscardDeviceResources()
{
    m_brushes.Reset();
    m_renderTarget.Reset();
}

void SettingsView::BuildActiveTabLayout(
    std::vector<BentoCard>& outCards,
    std::vector<CardLabel>& outLabels,
    std::vector<ControlItem>& outControls,
    std::vector<FrameBox>& outInputFrames,
    ScrollTextBox& outScrollBox)
{
    outCards.clear();
    outLabels.clear();
    outControls.clear();
    outInputFrames.clear();
    outScrollBox = {};

    if (m_activeTab == 0) // 파일
    {
        outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 304.0f, 366.0f), L"파일 및 미디어 열기" });
        outControls.push_back({ kIdFileOpen, D2D1::RectF(36.0f, 162.0f, 288.0f, 198.0f), L"동영상 파일 열기  (Ctrl+O)" });
        outControls.push_back({ kIdSubOpen, D2D1::RectF(36.0f, 210.0f, 288.0f, 246.0f), L"자막 파일 열기  (Ctrl+Shift+O)" });
        outControls.push_back({ kIdScreenshot, D2D1::RectF(36.0f, 258.0f, 288.0f, 294.0f), L"현재 화면 캡처  (PNG)" });
        outLabels.push_back({ D2D1::RectF(36.0f, 308.0f, 288.0f, 354.0f), L"지원 포맷: MP4, MKV, AVI, WMV,\nMOV, WEBM, FLV, TS, M4V", true });

        outCards.push_back({ D2D1::RectF(316.0f, 112.0f, 600.0f, 366.0f), L"연결 앱 및 종료" });
        outLabels.push_back({ D2D1::RectF(332.0f, 162.0f, 584.0f, 244.0f),
            L"파일 탐색기에서 동영상 우클릭 후 '연결 프로그램' 또는 Windows 기본 앱 설정에서 NextVideoPlayer를 지정할 수 있습니다.", true });
        outControls.push_back({ kIdExitApp, D2D1::RectF(332.0f, 258.0f, 584.0f, 294.0f), L"프로그램 종료  (Alt+F4)" });
    }
    else if (m_activeTab == 1) // 재생 (view_settings.png hero layout)
    {
        WCHAR stepBuf[32]{};
        swprintf_s(stepBuf, L"%d초", m_currentConfig.seekStepSeconds);

        outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 304.0f, 252.0f), L"탐색 간격", L"Seek Step" });
        outLabels.push_back({ D2D1::RectF(36.0f, 162.0f, 134.0f, 194.0f), L"방향키 이동" });
        outControls.push_back({ kIdSeekMinus, D2D1::RectF(140.0f, 162.0f, 176.0f, 194.0f), L"", ControlKind::StepperMinus });
        outControls.push_back({ kIdSeekDisplay, D2D1::RectF(176.0f, 162.0f, 252.0f, 194.0f), stepBuf, ControlKind::Display });
        outControls.push_back({ kIdSeekPlus, D2D1::RectF(252.0f, 162.0f, 288.0f, 194.0f), L"", ControlKind::StepperPlus });
        outLabels.push_back({ D2D1::RectF(36.0f, 206.0f, 288.0f, 240.0f), L"좌/우 방향키 1회 탐색 시간 (1~60초)", true });

        outCards.push_back({ D2D1::RectF(316.0f, 112.0f, 600.0f, 252.0f), L"반복 및 구간", L"Repeat & A-B" });
        outLabels.push_back({ D2D1::RectF(332.0f, 162.0f, 512.0f, 192.0f), L"전체 반복 재생" });
        outControls.push_back({ kIdToggleRepeat, D2D1::RectF(538.0f, 166.0f, 580.0f, 188.0f), L"", ControlKind::Toggle, m_currentConfig.repeatEnabled });
        outControls.push_back({ kIdABSetA, D2D1::RectF(332.0f, 206.0f, 408.0f, 238.0f), L"A 지점" });
        outControls.push_back({ kIdABSetB, D2D1::RectF(418.0f, 206.0f, 494.0f, 238.0f), L"B 지점" });
        outControls.push_back({ kIdABClear, D2D1::RectF(504.0f, 206.0f, 584.0f, 238.0f), L"해제 (0)" });

        outCards.push_back({ D2D1::RectF(20.0f, 264.0f, 304.0f, 408.0f), L"재생 목록", L"Playlist" });
        outControls.push_back({ kIdPrevFile, D2D1::RectF(36.0f, 316.0f, 156.0f, 352.0f), L"이전 파일", ControlKind::PrevSkip });
        outControls.push_back({ kIdNextFile, D2D1::RectF(168.0f, 316.0f, 288.0f, 352.0f), L"다음 파일", ControlKind::NextSkip });
        outLabels.push_back({ D2D1::RectF(36.0f, 366.0f, 288.0f, 396.0f), L"현재 재생목록 기준 이전/다음 미디어 전환", true });

        outCards.push_back({ D2D1::RectF(316.0f, 264.0f, 600.0f, 408.0f), L"재생 제어", L"Playback" });
        outLabels.push_back({ D2D1::RectF(332.0f, 314.0f, 512.0f, 344.0f), L"자막 화면 표시" });
        outControls.push_back({ kIdToggleSub, D2D1::RectF(538.0f, 318.0f, 580.0f, 340.0f), L"", ControlKind::Toggle, m_currentConfig.subtitleVisible });
        outControls.push_back({ kIdPlayPause, D2D1::RectF(332.0f, 358.0f, 466.0f, 394.0f), L"재생 / 일시정지" });
        outControls.push_back({ kIdStop, D2D1::RectF(478.0f, 358.0f, 584.0f, 394.0f), L"정지" });
    }
    else if (m_activeTab == 2) // 자막
    {
        WCHAR scaleBuf[32]{};
        const int pct = static_cast<int>(std::round(m_currentConfig.subtitleScale * 100.0));
        swprintf_s(scaleBuf, L"%d%%", pct);

        outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 304.0f, 354.0f), L"자막 표시 및 크기" });
        outLabels.push_back({ D2D1::RectF(36.0f, 162.0f, 224.0f, 194.0f), L"자막 화면 표시  (S)" });
        outControls.push_back({ kIdToggleSub, D2D1::RectF(246.0f, 167.0f, 288.0f, 189.0f), L"", ControlKind::Toggle, m_currentConfig.subtitleVisible });

        outLabels.push_back({ D2D1::RectF(36.0f, 214.0f, 134.0f, 246.0f), L"글자 크기" });
        outControls.push_back({ kIdSubScaleMinus, D2D1::RectF(140.0f, 214.0f, 176.0f, 246.0f), L"", ControlKind::StepperMinus });
        outControls.push_back({ kIdSubScaleDisplay, D2D1::RectF(176.0f, 214.0f, 252.0f, 246.0f), scaleBuf, ControlKind::Display });
        outControls.push_back({ kIdSubScalePlus, D2D1::RectF(252.0f, 214.0f, 288.0f, 246.0f), L"", ControlKind::StepperPlus });
        outLabels.push_back({ D2D1::RectF(36.0f, 266.0f, 288.0f, 316.0f), L"배율 범위: 20% ~ 300% (10% 단위 조절)", true });

        outCards.push_back({ D2D1::RectF(316.0f, 112.0f, 600.0f, 354.0f), L"싱크 및 위치 조절" });
        outLabels.push_back({ D2D1::RectF(332.0f, 162.0f, 584.0f, 190.0f), L"자막 싱크 미세 조절" });
        outControls.push_back({ kIdSubDelayMinus, D2D1::RectF(332.0f, 198.0f, 452.0f, 234.0f), L"-0.5초 빠르게" });
        outControls.push_back({ kIdSubDelayPlus, D2D1::RectF(464.0f, 198.0f, 584.0f, 234.0f), L"+0.5초 느리게" });

        outLabels.push_back({ D2D1::RectF(332.0f, 252.0f, 584.0f, 280.0f), L"자막 세로 위치" });
        outControls.push_back({ kIdSubPosUp, D2D1::RectF(332.0f, 288.0f, 452.0f, 324.0f), L"위치 위로", ControlKind::UpArrow });
        outControls.push_back({ kIdSubPosDown, D2D1::RectF(464.0f, 288.0f, 584.0f, 324.0f), L"위치 아래로", ControlKind::DownArrow });
    }
    else if (m_activeTab == 3) // 보기
    {
        outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 600.0f, 252.0f), L"창 및 화면 모드" });
        outLabels.push_back({ D2D1::RectF(36.0f, 162.0f, 512.0f, 194.0f), L"항상 위에 표시  (Always on Top)" });
        outControls.push_back({ kIdToggleTopMost, D2D1::RectF(540.0f, 167.0f, 582.0f, 189.0f), L"", ControlKind::Toggle, m_currentConfig.topMost });
        outControls.push_back({ kIdFullscreen, D2D1::RectF(36.0f, 204.0f, 304.0f, 238.0f), L"전체 화면 전환  (Enter / F11)" });
        outControls.push_back({ kIdPlaylist, D2D1::RectF(316.0f, 204.0f, 584.0f, 238.0f), L"재생목록 창 열기  (F3)" });

        outCards.push_back({ D2D1::RectF(20.0f, 264.0f, 600.0f, 384.0f), L"화면 제어 안내" });
        outLabels.push_back({ D2D1::RectF(36.0f, 314.0f, 584.0f, 372.0f),
            L"- 전체 화면에서는 마우스 커서를 화면 하단으로 내리면 컨트롤 바가 나타납니다.\n- 재생목록 창(F3)은 메인 창과 별도로 배치하고 드래그하여 크기 조절이 가능합니다.", true });
    }
    else if (m_activeTab == 4) // API
    {
        if (m_showingApiGuide)
        {
            outControls.push_back({ kIdApiGuide, D2D1::RectF(20.0f, 112.0f, 204.0f, 144.0f), L"API 설정으로 돌아가기" });
            outControls.push_back({ kIdApiCopyGuide, D2D1::RectF(216.0f, 112.0f, 396.0f, 144.0f), L"가이드 전체 복사", ControlKind::Button, false, false, true });
            outControls.push_back({ kIdApiOpenReadme, D2D1::RectF(408.0f, 112.0f, 600.0f, 144.0f), L"API/README.md 파일 열기" });

            outCards.push_back({ D2D1::RectF(20.0f, 154.0f, 600.0f, 412.0f), L"WebSocket API 상세 가이드", L"API/README.md" });
            outScrollBox.active = true;
            outScrollBox.containerRect = D2D1::RectF(34.0f, 198.0f, 586.0f, 400.0f);
            outScrollBox.text = kApiGuideFullText;
            outScrollBox.scrollOffset = m_apiGuideScroll;
            PrepareScrollBoxMetrics(outScrollBox);
            m_apiGuideScroll = outScrollBox.scrollOffset;
        }
        else
        {
            outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 332.0f, 248.0f), L"HTTP / WebSocket 서버" });
            outLabels.push_back({ D2D1::RectF(36.0f, 150.0f, 256.0f, 174.0f), L"외부 제어 API 활성화" });
            outControls.push_back({ kIdApiEnable, D2D1::RectF(274.0f, 151.0f, 316.0f, 173.0f), L"", ControlKind::Toggle, m_currentConfig.api.enabled });

            outLabels.push_back({ D2D1::RectF(36.0f, 180.0f, 110.0f, 204.0f), L"바인딩 IP" });
            outInputFrames.push_back({ D2D1::RectF(116.0f, 178.0f, 316.0f, 206.0f), m_ipFocused });

            outLabels.push_back({ D2D1::RectF(36.0f, 214.0f, 110.0f, 238.0f), L"포트 번호" });
            outInputFrames.push_back({ D2D1::RectF(116.0f, 212.0f, 216.0f, 240.0f), m_portFocused });
            outLabels.push_back({ D2D1::RectF(224.0f, 214.0f, 320.0f, 238.0f), L"(기본 49152)", true });

            std::wstring statusText;
            if (m_apiStatusProvider)
            {
                statusText = m_apiStatusProvider();
            }
            else
            {
                statusText = L"ws" + m_currentConfig.api.HttpUrl().substr(4) + L"api/v1";
            }

            outCards.push_back({ D2D1::RectF(344.0f, 112.0f, 600.0f, 248.0f), L"빠른 연결 및 제어" });
            outLabels.push_back({ D2D1::RectF(358.0f, 150.0f, 586.0f, 174.0f), statusText, false });
            outControls.push_back({ kIdApiPage, D2D1::RectF(358.0f, 178.0f, 586.0f, 206.0f), L"켜고 테스트창 열기", ControlKind::Button, false, false, true });
            outControls.push_back({ kIdApiCopy, D2D1::RectF(358.0f, 212.0f, 586.0f, 240.0f), L"WebSocket 주소 복사" });

            outCards.push_back({ D2D1::RectF(20.0f, 258.0f, 600.0f, 412.0f), L"API 빠른 가이드 (스크롤 가능)", L"v1 JSON" });
            outScrollBox.active = true;
            outScrollBox.containerRect = D2D1::RectF(34.0f, 300.0f, 444.0f, 400.0f);
            outScrollBox.text = kApiGuideFullText;
            outScrollBox.scrollOffset = m_apiSummaryScroll;
            PrepareScrollBoxMetrics(outScrollBox);
            m_apiSummaryScroll = outScrollBox.scrollOffset;

            outControls.push_back({ kIdApiGuide, D2D1::RectF(456.0f, 300.0f, 586.0f, 330.0f), L"상세 가이드 확대", ControlKind::Button, false, false, true });
            outControls.push_back({ kIdApiCopyGuide, D2D1::RectF(456.0f, 335.0f, 586.0f, 365.0f), L"가이드 복사" });
            outControls.push_back({ kIdApiOpenReadme, D2D1::RectF(456.0f, 370.0f, 586.0f, 400.0f), L"README.md 열기" });
        }
    }
    else if (m_activeTab == 5) // 정보
    {
        outCards.push_back({ D2D1::RectF(20.0f, 112.0f, 600.0f, 364.0f),
                             m_showingLicenses ? L"오픈소스 라이선스 전문" : L"NextVideoPlayer 정보" });
        outScrollBox.active = true;
        outScrollBox.containerRect = D2D1::RectF(34.0f, 156.0f, 586.0f, 352.0f);
        outScrollBox.text = m_showingLicenses ? AppInfo::LicenseText(m_hInstance) : m_appInfo.displayText;
        outScrollBox.scrollOffset = m_infoScroll;
        PrepareScrollBoxMetrics(outScrollBox);
        m_infoScroll = outScrollBox.scrollOffset;

        outControls.push_back({ kIdInfoEmail, D2D1::RectF(20.0f, 374.0f, 204.0f, 408.0f), L"이메일 문의" });
        outControls.push_back({ kIdInfoYoutube, D2D1::RectF(216.0f, 374.0f, 400.0f, 408.0f),
                               m_appInfo.youtubeUrl.empty() ? L"YouTube 준비 중" : L"YouTube 채널",
                               ControlKind::Button, false, m_appInfo.youtubeUrl.empty(), false });
        outControls.push_back({ kIdInfoLicense, D2D1::RectF(412.0f, 374.0f, 600.0f, 408.0f),
                               m_showingLicenses ? L"앱 정보로 돌아가기" : L"라이선스 전문",
                               ControlKind::Button, false, false, true });
    }
}

void SettingsView::PrepareScrollBoxMetrics(ScrollTextBox& box) const
{
    if (!box.active) return;

    const float padX = 12.0f;
    const float padY = 10.0f;
    const float scrollColW = 12.0f;
    box.textRect = D2D1::RectF(
        box.containerRect.left + padX,
        box.containerRect.top + padY,
        box.containerRect.right - padX - scrollColW,
        box.containerRect.bottom - padY);

    const float viewW = (std::max)(20.0f, box.textRect.right - box.textRect.left);
    const float viewH = (std::max)(20.0f, box.textRect.bottom - box.textRect.top);

    box.contentHeight = viewH;
    if (m_dwriteFactory && m_scrollTextFormat && !box.text.empty())
    {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(
                box.text.c_str(),
                static_cast<UINT32>(box.text.length()),
                m_scrollTextFormat.Get(),
                viewW,
                8000.0f,
                layout.GetAddressOf())))
        {
            DWRITE_TEXT_METRICS metrics{};
            if (SUCCEEDED(layout->GetMetrics(&metrics)))
            {
                box.contentHeight = metrics.height;
            }
        }
    }

    box.maxScroll = (std::max)(0.0f, box.contentHeight - viewH);
    box.scrollOffset = std::clamp(box.scrollOffset, 0.0f, box.maxScroll);

    box.trackRect = D2D1::RectF(
        box.containerRect.right - 9.0f,
        box.containerRect.top + 8.0f,
        box.containerRect.right - 5.0f,
        box.containerRect.bottom - 8.0f);

    if (box.maxScroll > 0.5f)
    {
        const float trackH = box.trackRect.bottom - box.trackRect.top;
        const float ratio = std::clamp(viewH / (std::max)(viewH + 1.0f, box.contentHeight), 0.1f, 1.0f);
        const float thumbH = (std::max)(24.0f, trackH * ratio);
        const float travel = (std::max)(0.0f, trackH - thumbH);
        const float thumbTop = box.trackRect.top + (box.scrollOffset / box.maxScroll) * travel;
        box.thumbRect = D2D1::RectF(box.trackRect.left, thumbTop, box.trackRect.right, thumbTop + thumbH);
    }
    else
    {
        box.thumbRect = {};
    }
}

void SettingsView::DrawScrollBox(ID2D1RenderTarget* rt, const ScrollTextBox& box) const
{
    if (!box.active) return;

    D2D1_ROUNDED_RECT insetRr = D2D1::RoundedRect(box.containerRect, 7.0f, 7.0f);
    rt->FillRoundedRectangle(insetRr, m_brushes.insetBg.Get());
    rt->DrawRoundedRectangle(insetRr, m_brushes.insetBorder.Get(), 1.0f);

    const float viewW = (std::max)(20.0f, box.textRect.right - box.textRect.left);
    if (m_dwriteFactory && m_scrollTextFormat && !box.text.empty())
    {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        if (SUCCEEDED(m_dwriteFactory->CreateTextLayout(
                box.text.c_str(),
                static_cast<UINT32>(box.text.length()),
                m_scrollTextFormat.Get(),
                viewW,
                8000.0f,
                layout.GetAddressOf())))
        {
            rt->PushAxisAlignedClip(box.textRect, D2D1_ANTIALIAS_MODE_ALIASED);
            rt->DrawTextLayout(
                D2D1::Point2F(box.textRect.left, box.textRect.top - box.scrollOffset),
                layout.Get(),
                m_brushes.codeText.Get());
            rt->PopAxisAlignedClip();
        }
    }

    if (box.maxScroll > 0.5f)
    {
        rt->FillRoundedRectangle(D2D1::RoundedRect(box.trackRect, 2.0f, 2.0f), m_brushes.scrollTrack.Get());
        const bool activeThumb = m_isDraggingScroll || (m_hoveredId == kIdScrollArea);
        rt->FillRoundedRectangle(
            D2D1::RoundedRect(box.thumbRect, 2.0f, 2.0f),
            activeThumb ? m_brushes.cyan.Get() : m_brushes.scrollThumb.Get());
    }
}

int SettingsView::HitTestControl(float x, float y) const
{
    if (PtInRectF(GetCloseButtonRect(), x, y)) return kIdCloseBtn;

    for (int i = 0; i < kTabCount; ++i)
    {
        if (PtInRectF(GetTabRect(i), x, y)) return kIdTabFile + i;
    }

    std::vector<BentoCard> cards;
    std::vector<CardLabel> labels;
    std::vector<ControlItem> controls;
    std::vector<FrameBox> inputFrames;
    ScrollTextBox scrollBox;
    const_cast<SettingsView*>(this)->BuildActiveTabLayout(cards, labels, controls, inputFrames, scrollBox);

    if (scrollBox.active && scrollBox.maxScroll > 0.5f)
    {
        D2D1_RECT_F scrollHit = D2D1::RectF(
            scrollBox.trackRect.left - 6.0f,
            scrollBox.trackRect.top,
            scrollBox.trackRect.right + 4.0f,
            scrollBox.trackRect.bottom);
        if (PtInRectF(scrollHit, x, y)) return kIdScrollArea;
    }

    for (const auto& ctrl : controls)
    {
        if (!ctrl.disabled && ctrl.kind != ControlKind::Display && PtInRectF(ctrl.rect, x, y))
        {
            return ctrl.id;
        }
    }

    if (PtInRectF(GetSaveButtonRect(), x, y)) return kIdBtnSave;
    if (PtInRectF(GetCancelButtonRect(), x, y)) return kIdBtnCancel;
    return 0;
}

void SettingsView::DispatchParentCommand(int commandId, bool closeAfter)
{
    if (closeAfter)
    {
        m_pendingPostCommand = commandId;
        CloseModal(true);
        return;
    }

    if (m_parentHwnd && IsWindow(m_parentHwnd))
    {
        EnableWindow(m_parentHwnd, TRUE);
        SendMessageW(m_parentHwnd, WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
        if (m_hWnd && IsWindow(m_hWnd))
        {
            EnableWindow(m_parentHwnd, FALSE);
            SetForegroundWindow(m_hWnd);
        }
    }
}

void SettingsView::ExecuteControl(int controlId)
{
    switch (controlId)
    {
    case kIdCloseBtn:
    case kIdBtnCancel:
        if (m_onCancelClicked) m_onCancelClicked();
        else CloseModal(false);
        break;
    case kIdBtnSave:
        if (m_onSaveClicked) m_onSaveClicked();
        else CloseModal(true);
        break;
    case kIdTabFile:
    case kIdTabPlayback:
    case kIdTabSubtitle:
    case kIdTabView:
    case kIdTabApi:
    case kIdTabInfo:
        m_activeTab = controlId - kIdTabFile;
        m_isDraggingScroll = false;
        UpdateTabVisibility();
        break;
    case kIdApiEnable:
        m_currentConfig.api.enabled = !m_currentConfig.api.enabled;
        break;
    case kIdApiPage:
        m_currentConfig.api.enabled = true;
        if (m_onApiPageRequested) m_onApiPageRequested();
        break;
    case kIdApiCopy:
        CopyApiUrl();
        break;
    case kIdApiGuide:
        ToggleApiGuide();
        break;
    case kIdApiCopyGuide:
        CopyTextWithFeedback(kApiGuideFullText, L"API 가이드 전체 내용이 클립보드에 복사되었습니다.");
        break;
    case kIdApiOpenReadme:
        OpenApiReadme();
        break;
    case kIdInfoEmail:
        OpenInfoLink(L"mailto:" + m_appInfo.email + L"?subject=NextVideoPlayer");
        break;
    case kIdInfoYoutube:
        if (!m_appInfo.youtubeUrl.empty())
            OpenInfoLink(m_appInfo.youtubeUrl);
        break;
    case kIdInfoLicense:
        ToggleLicenses();
        break;

    case kIdFileOpen:
        DispatchParentCommand(IDM_FILE_OPEN);
        break;
    case kIdSubOpen:
        DispatchParentCommand(IDM_SUBTITLE_OPEN);
        break;
    case kIdScreenshot:
        DispatchParentCommand(IDM_SCREENSHOT);
        break;
    case kIdExitApp:
        DispatchParentCommand(IDM_EXIT, true);
        break;

    case kIdSeekMinus:
        m_currentConfig.seekStepSeconds = (std::max)(1, m_currentConfig.seekStepSeconds - 1);
        break;
    case kIdSeekPlus:
        m_currentConfig.seekStepSeconds = (std::min)(60, m_currentConfig.seekStepSeconds + 1);
        break;
    case kIdToggleRepeat:
        m_currentConfig.repeatEnabled = !m_currentConfig.repeatEnabled;
        break;
    case kIdABSetA:
        DispatchParentCommand(IDM_AB_SET_A);
        break;
    case kIdABSetB:
        DispatchParentCommand(IDM_AB_SET_B);
        break;
    case kIdABClear:
        DispatchParentCommand(IDM_AB_CLEAR);
        break;
    case kIdPrevFile:
        DispatchParentCommand(IDM_PREV_FILE);
        break;
    case kIdPlayPause:
        DispatchParentCommand(IDM_PLAY_PAUSE);
        break;
    case kIdStop:
        DispatchParentCommand(IDM_STOP);
        break;
    case kIdNextFile:
        DispatchParentCommand(IDM_NEXT_FILE);
        break;

    case kIdToggleSub:
        m_currentConfig.subtitleVisible = !m_currentConfig.subtitleVisible;
        break;
    case kIdSubScaleMinus:
        m_currentConfig.subtitleScale = std::clamp(m_currentConfig.subtitleScale - 0.1, 0.2, 3.0);
        DispatchParentCommand(IDM_SUB_SCALE_MINUS);
        break;
    case kIdSubScalePlus:
        m_currentConfig.subtitleScale = std::clamp(m_currentConfig.subtitleScale + 0.1, 0.2, 3.0);
        DispatchParentCommand(IDM_SUB_SCALE_PLUS);
        break;
    case kIdSubDelayMinus:
        DispatchParentCommand(IDM_SUB_DELAY_MINUS);
        break;
    case kIdSubDelayPlus:
        DispatchParentCommand(IDM_SUB_DELAY_PLUS);
        break;
    case kIdSubPosUp:
        DispatchParentCommand(IDM_SUB_POS_UP);
        break;
    case kIdSubPosDown:
        DispatchParentCommand(IDM_SUB_POS_DOWN);
        break;

    case kIdToggleTopMost:
        m_currentConfig.topMost = !m_currentConfig.topMost;
        break;
    case kIdFullscreen:
        DispatchParentCommand(IDM_VIEW_FULLSCREEN, true);
        break;
    case kIdPlaylist:
        DispatchParentCommand(IDM_VIEW_PLAYLIST, true);
        break;
    }

    if (m_hWnd && IsWindow(m_hWnd))
    {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void SettingsView::DrawGearIcon(ID2D1RenderTarget* rt, float cx, float cy, float radius) const
{
    for (int i = 0; i < 8; ++i)
    {
        const float angle = i * 3.14159265f / 4.0f;
        const float x1 = cx + std::cos(angle) * (radius * 0.62f);
        const float y1 = cy + std::sin(angle) * (radius * 0.62f);
        const float x2 = cx + std::cos(angle) * (radius * 1.05f);
        const float y2 = cy + std::sin(angle) * (radius * 1.05f);
        rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m_brushes.subText.Get(), 2.2f);
    }
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.64f, radius * 0.64f), m_brushes.bg.Get());
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.64f, radius * 0.64f), m_brushes.subText.Get(), 1.7f);
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.25f, radius * 0.25f), m_brushes.cyan.Get(), 1.4f);
}

void SettingsView::DrawCloseIcon(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, ID2D1Brush* brush) const
{
    const float cx = (rect.left + rect.right) * 0.5f;
    const float cy = (rect.top + rect.bottom) * 0.5f;
    const float r = 4.6f;
    rt->DrawLine(D2D1::Point2F(cx - r, cy - r), D2D1::Point2F(cx + r, cy + r), brush, 1.6f);
    rt->DrawLine(D2D1::Point2F(cx + r, cy - r), D2D1::Point2F(cx - r, cy + r), brush, 1.6f);
}

void SettingsView::DrawStepperGlyph(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool isPlus, ID2D1Brush* brush) const
{
    const float cx = (rect.left + rect.right) * 0.5f;
    const float cy = (rect.top + rect.bottom) * 0.5f;
    const float r = 4.5f;
    rt->DrawLine(D2D1::Point2F(cx - r, cy), D2D1::Point2F(cx + r, cy), brush, 1.6f);
    if (isPlus)
    {
        rt->DrawLine(D2D1::Point2F(cx, cy - r), D2D1::Point2F(cx, cy + r), brush, 1.6f);
    }
}

void SettingsView::DrawSkipGlyph(ID2D1RenderTarget* rt, float cx, float cy, bool isNext, ID2D1Brush* brush) const
{
    Microsoft::WRL::ComPtr<ID2D1Factory> factory;
    rt->GetFactory(factory.GetAddressOf());
    Microsoft::WRL::ComPtr<ID2D1PathGeometry> path;
    if (factory && SUCCEEDED(factory->CreatePathGeometry(path.GetAddressOf())))
    {
        Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
        if (SUCCEEDED(path->Open(sink.GetAddressOf())))
        {
            if (isNext)
            {
                sink->BeginFigure(D2D1::Point2F(cx - 4.0f, cy - 4.5f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx + 2.0f, cy));
                sink->AddLine(D2D1::Point2F(cx - 4.0f, cy + 4.5f));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
            else
            {
                sink->BeginFigure(D2D1::Point2F(cx + 4.0f, cy - 4.5f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx - 2.0f, cy));
                sink->AddLine(D2D1::Point2F(cx + 4.0f, cy + 4.5f));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
            sink->Close();
            rt->FillGeometry(path.Get(), brush);
        }
    }
    const float barX = isNext ? (cx + 3.8f) : (cx - 3.8f);
    rt->DrawLine(D2D1::Point2F(barX, cy - 4.5f), D2D1::Point2F(barX, cy + 4.5f), brush, 1.5f);
}

void SettingsView::DrawToggleSwitch(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool state, bool hovered) const
{
    const float radius = (rect.bottom - rect.top) * 0.5f;
    const D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(rect, radius, radius);
    const float cy = (rect.top + rect.bottom) * 0.5f;

    if (state)
    {
        D2D1_ROUNDED_RECT glow = D2D1::RoundedRect(
            D2D1::RectF(rect.left - 2.0f, rect.top - 2.0f, rect.right + 2.0f, rect.bottom + 2.0f),
            radius + 2.0f, radius + 2.0f);
        rt->DrawRoundedRectangle(glow, m_brushes.cyanGlow.Get(), 3.2f);
        rt->FillRoundedRectangle(pill, m_brushes.cyan.Get());

        const float knobX = rect.right - radius;
        const float knobR = radius - 3.2f;
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, cy), knobR, knobR), m_brushes.whiteKnob.Get());
    }
    else
    {
        rt->FillRoundedRectangle(pill, m_brushes.toggleOffBg.Get());
        rt->DrawRoundedRectangle(pill, hovered ? m_brushes.cyan.Get() : m_brushes.toggleOffBorder.Get(), 1.2f);

        const float knobX = rect.left + radius;
        const float knobR = radius - 3.5f;
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(knobX, cy), knobR, knobR), m_brushes.toggleOffKnob.Get());
    }
}

void SettingsView::Draw()
{
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    if (FAILED(CreateDeviceResources())) return;

    m_renderTarget->BeginDraw();
    DrawToTarget(m_renderTarget.Get(), m_renderTarget->GetSize(), false);

    if (m_renderTarget->EndDraw() == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void SettingsView::DrawToTarget(ID2D1RenderTarget* rt, D2D1_SIZE_F size, bool isPreview)
{
    if (!rt) return;
    if (isPreview) m_brushes.Reset();
    if (FAILED(CreateDeviceIndependentResources()) || FAILED(m_brushes.Ensure(rt))) return;

    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    rt->Clear(D2D1::ColorF(0x161922, 1.0f));

    // Outer subtle modal frame
    rt->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, size.width - 0.5f, size.height - 0.5f), 8.0f, 8.0f),
        m_brushes.windowBorder.Get(),
        1.0f);

    // 1. Header
    DrawGearIcon(rt, 34.0f, kHeaderHeight * 0.5f, 9.0f);
    rt->DrawTextW(L"환경설정", 4, m_titleFormat.Get(),
        D2D1::RectF(52.0f, 8.0f, 126.0f, kHeaderHeight - 6.0f), m_brushes.text.Get());
    rt->DrawTextW(L"(Preferences)", 13, m_subTitleFormat.Get(),
        D2D1::RectF(126.0f, 10.0f, 240.0f, kHeaderHeight - 6.0f), m_brushes.mutedText.Get());

    const D2D1_RECT_F rcClose = GetCloseButtonRect();
    if (m_hoveredId == kIdCloseBtn)
    {
        rt->FillRoundedRectangle(D2D1::RoundedRect(rcClose, 7.0f, 7.0f), m_brushes.closeHover.Get());
    }
    DrawCloseIcon(rt, rcClose, (m_hoveredId == kIdCloseBtn) ? m_brushes.text.Get() : m_brushes.subText.Get());

    rt->DrawLine(D2D1::Point2F(20.0f, kHeaderHeight), D2D1::Point2F(size.width - 20.0f, kHeaderHeight),
        m_brushes.divider.Get(), 1.0f);

    // 2. Pill Tab Strip
    const D2D1_ROUNDED_RECT tabTrack = D2D1::RoundedRect(
        D2D1::RectF(20.0f, kTabBarTop, size.width - 20.0f, kTabBarBottom), 8.0f, 8.0f);
    rt->FillRoundedRectangle(tabTrack, m_brushes.tabTrackBg.Get());
    rt->DrawRoundedRectangle(tabTrack, m_brushes.tabTrackBorder.Get(), 1.0f);

    const wchar_t* tabNames[kTabCount] = { L"파일", L"재생", L"자막", L"보기", L"API", L"정보" };
    for (int i = 0; i < kTabCount; ++i)
    {
        const D2D1_RECT_F rcTab = GetTabRect(i);
        const bool isActive = (m_activeTab == i);
        const bool isHover = (m_hoveredId == kIdTabFile + i);

        if (isActive)
        {
            const D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(rcTab, 6.0f, 6.0f);
            rt->FillRoundedRectangle(pill, m_brushes.activeTabBg.Get());
            rt->DrawRoundedRectangle(pill, m_brushes.activeTabBorder.Get(), 1.0f);

            const float indW = 42.0f;
            const float midX = (rcTab.left + rcTab.right) * 0.5f;
            const D2D1_RECT_F glowRc = D2D1::RectF(midX - indW * 0.5f - 2.0f, rcTab.bottom - 3.5f, midX + indW * 0.5f + 2.0f, rcTab.bottom);
            rt->FillRoundedRectangle(D2D1::RoundedRect(glowRc, 1.5f, 1.5f), m_brushes.cyanGlow.Get());
            const D2D1_RECT_F barRc = D2D1::RectF(midX - indW * 0.5f, rcTab.bottom - 2.5f, midX + indW * 0.5f, rcTab.bottom);
            rt->FillRoundedRectangle(D2D1::RoundedRect(barRc, 1.2f, 1.2f), m_brushes.cyan.Get());
        }
        else if (isHover)
        {
            rt->FillRoundedRectangle(D2D1::RoundedRect(rcTab, 6.0f, 6.0f), m_brushes.hoverTabBg.Get());
        }

        rt->DrawTextW(
            tabNames[i],
            static_cast<UINT32>(wcslen(tabNames[i])),
            isActive ? m_tabBoldFormat.Get() : m_tabFormat.Get(),
            rcTab,
            isActive ? m_brushes.text.Get() : (isHover ? m_brushes.subText.Get() : m_brushes.mutedText.Get()));
    }

    // 3. Content Layout
    std::vector<BentoCard> cards;
    std::vector<CardLabel> labels;
    std::vector<ControlItem> controls;
    std::vector<FrameBox> inputFrames;
    ScrollTextBox scrollBox;
    BuildActiveTabLayout(cards, labels, controls, inputFrames, scrollBox);

    for (const auto& card : cards)
    {
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(card.rect, 10.0f, 10.0f);
        rt->FillRoundedRectangle(rr, m_brushes.cardBg.Get());
        rt->DrawRoundedRectangle(rr, m_brushes.cardBorder.Get(), 1.0f);

        const float headerY = card.rect.top + 38.0f;
        rt->DrawTextW(
            card.title.c_str(),
            static_cast<UINT32>(card.title.length()),
            m_cardHeaderFormat.Get(),
            D2D1::RectF(card.rect.left + 16.0f, card.rect.top + 8.0f, card.rect.right - 90.0f, headerY - 2.0f),
            m_brushes.text.Get());

        if (!card.subtitle.empty())
        {
            rt->DrawTextW(
                card.subtitle.c_str(),
                static_cast<UINT32>(card.subtitle.length()),
                m_cardSubTitleFormat.Get(),
                D2D1::RectF(card.rect.right - 130.0f, card.rect.top + 8.0f, card.rect.right - 16.0f, headerY - 2.0f),
                m_brushes.mutedText.Get());
        }

        rt->DrawLine(
            D2D1::Point2F(card.rect.left + 14.0f, headerY),
            D2D1::Point2F(card.rect.right - 14.0f, headerY),
            m_brushes.divider.Get(),
            1.0f);
    }

    for (const auto& frame : inputFrames)
    {
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(frame.rect, 6.0f, 6.0f);
        rt->FillRoundedRectangle(rr, m_brushes.insetBg.Get());
        rt->DrawRoundedRectangle(rr, frame.focused ? m_brushes.cyan.Get() : m_brushes.insetBorder.Get(), frame.focused ? 1.4f : 1.0f);
    }

    if (isPreview && inputFrames.size() >= 2)
    {
        rt->DrawTextW(m_currentConfig.api.ip.c_str(), static_cast<UINT32>(m_currentConfig.api.ip.length()),
            m_subTitleFormat.Get(), D2D1::RectF(inputFrames[0].rect.left + 10.0f, inputFrames[0].rect.top, inputFrames[0].rect.right - 8.0f, inputFrames[0].rect.bottom),
            m_brushes.text.Get());
        const auto portStr = std::to_wstring(m_currentConfig.api.port);
        rt->DrawTextW(portStr.c_str(), static_cast<UINT32>(portStr.length()),
            m_subTitleFormat.Get(), D2D1::RectF(inputFrames[1].rect.left + 10.0f, inputFrames[1].rect.top, inputFrames[1].rect.right - 8.0f, inputFrames[1].rect.bottom),
            m_brushes.text.Get());
    }

    for (const auto& lbl : labels)
    {
        IDWriteTextFormat* fmt = lbl.muted ? m_captionFormat.Get()
            : (lbl.bold ? m_labelBoldFormat.Get() : ((m_activeTab == 4 && !m_showingApiGuide && lbl.rect.left > 340.0f) ? m_subTitleFormat.Get() : m_labelFormat.Get()));
        ID2D1Brush* br = (m_activeTab == 4 && !m_showingApiGuide && lbl.rect.left > 340.0f)
            ? m_brushes.subText.Get()
            : (lbl.muted ? m_brushes.mutedText.Get() : m_brushes.text.Get());
        rt->DrawTextW(
            lbl.text.c_str(),
            static_cast<UINT32>(lbl.text.length()),
            fmt,
            lbl.rect,
            br);
    }

    if (scrollBox.active)
    {
        DrawScrollBox(rt, scrollBox);
    }

    for (const auto& ctrl : controls)
    {
        const bool hover = (!ctrl.disabled && m_hoveredId == ctrl.id);
        const bool press = (!ctrl.disabled && m_pressedId == ctrl.id);

        if (ctrl.kind == ControlKind::Toggle)
        {
            DrawToggleSwitch(rt, ctrl.rect, ctrl.state, hover);
            continue;
        }

        if (ctrl.kind == ControlKind::Display)
        {
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(ctrl.rect, 0.0f, 0.0f);
            rt->FillRoundedRectangle(rr, m_brushes.insetBg.Get());
            rt->DrawLine(D2D1::Point2F(ctrl.rect.left, ctrl.rect.top + 0.5f), D2D1::Point2F(ctrl.rect.right, ctrl.rect.top + 0.5f), m_brushes.btnBorder.Get(), 1.0f);
            rt->DrawLine(D2D1::Point2F(ctrl.rect.left, ctrl.rect.bottom - 0.5f), D2D1::Point2F(ctrl.rect.right, ctrl.rect.bottom - 0.5f), m_brushes.btnBorder.Get(), 1.0f);
            rt->DrawTextW(ctrl.text.c_str(), static_cast<UINT32>(ctrl.text.length()), m_btnBoldFormat.Get(), ctrl.rect, m_brushes.text.Get());
            continue;
        }

        const float cornerR = (ctrl.kind == ControlKind::StepperMinus || ctrl.kind == ControlKind::StepperPlus) ? 6.0f : 7.0f;
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(ctrl.rect, cornerR, cornerR);

        ID2D1Brush* bgBrush = m_brushes.btnBg.Get();
        ID2D1Brush* borderBrush = m_brushes.btnBorder.Get();
        ID2D1Brush* fgBrush = m_brushes.text.Get();

        if (ctrl.disabled)
        {
            fgBrush = m_brushes.mutedText.Get();
        }
        else if (ctrl.accent)
        {
            bgBrush = press ? m_brushes.btnPressed.Get() : (hover ? m_brushes.accentBtnHover.Get() : m_brushes.accentBtnBg.Get());
            borderBrush = m_brushes.cyan.Get();
        }
        else
        {
            bgBrush = press ? m_brushes.btnPressed.Get() : (hover ? m_brushes.btnHover.Get() : m_brushes.btnBg.Get());
            borderBrush = hover ? m_brushes.cyan.Get() : m_brushes.btnBorder.Get();
        }

        rt->FillRoundedRectangle(rr, bgBrush);
        rt->DrawRoundedRectangle(rr, borderBrush, 1.0f);

        if (ctrl.kind == ControlKind::StepperMinus || ctrl.kind == ControlKind::StepperPlus)
        {
            DrawStepperGlyph(rt, ctrl.rect, ctrl.kind == ControlKind::StepperPlus, hover ? m_brushes.cyan.Get() : fgBrush);
        }
        else if (ctrl.kind == ControlKind::PrevSkip || ctrl.kind == ControlKind::NextSkip)
        {
            const bool isNext = (ctrl.kind == ControlKind::NextSkip);
            const float midY = (ctrl.rect.top + ctrl.rect.bottom) * 0.5f;
            const float glyphX = isNext ? (ctrl.rect.right - 18.0f) : (ctrl.rect.left + 18.0f);
            const D2D1_RECT_F txtRc = isNext
                ? D2D1::RectF(ctrl.rect.left + 8.0f, ctrl.rect.top, ctrl.rect.right - 26.0f, ctrl.rect.bottom)
                : D2D1::RectF(ctrl.rect.left + 26.0f, ctrl.rect.top, ctrl.rect.right - 8.0f, ctrl.rect.bottom);
            DrawSkipGlyph(rt, glyphX, midY, isNext, hover ? m_brushes.cyan.Get() : fgBrush);
            rt->DrawTextW(ctrl.text.c_str(), static_cast<UINT32>(ctrl.text.length()), m_btnFormat.Get(), txtRc, fgBrush);
        }
        else if (ctrl.kind == ControlKind::UpArrow || ctrl.kind == ControlKind::DownArrow)
        {
            const bool up = (ctrl.kind == ControlKind::UpArrow);
            const float midY = (ctrl.rect.top + ctrl.rect.bottom) * 0.5f;
            const float gx = ctrl.rect.right - 18.0f;
            const float dy = up ? -3.0f : 3.0f;
            rt->DrawLine(D2D1::Point2F(gx - 4.0f, midY - dy), D2D1::Point2F(gx, midY + dy), fgBrush, 1.5f);
            rt->DrawLine(D2D1::Point2F(gx, midY + dy), D2D1::Point2F(gx + 4.0f, midY - dy), fgBrush, 1.5f);
            rt->DrawTextW(ctrl.text.c_str(), static_cast<UINT32>(ctrl.text.length()), m_btnFormat.Get(),
                D2D1::RectF(ctrl.rect.left + 6.0f, ctrl.rect.top, ctrl.rect.right - 24.0f, ctrl.rect.bottom), fgBrush);
        }
        else
        {
            rt->DrawTextW(ctrl.text.c_str(), static_cast<UINT32>(ctrl.text.length()),
                ctrl.accent ? m_btnBoldFormat.Get() : m_btnFormat.Get(), ctrl.rect, fgBrush);
        }
    }

    // 4. Footer
    rt->DrawLine(D2D1::Point2F(20.0f, kFooterTop), D2D1::Point2F(size.width - 20.0f, kFooterTop),
        m_brushes.divider.Get(), 1.0f);

    const bool hasFeedback = !m_footerFeedback.empty() && GetTickCount64() < m_feedbackExpireTick;
    const std::wstring footerHint = hasFeedback ? m_footerFeedback : L"ESC 닫기  ·  변경 후 [저장]을 누르면 즉시 반영됩니다.";
    rt->DrawTextW(
        footerHint.c_str(),
        static_cast<UINT32>(footerHint.length()),
        m_subTitleFormat.Get(),
        D2D1::RectF(20.0f, 436.0f, 392.0f, 470.0f),
        hasFeedback ? m_brushes.cyan.Get() : m_brushes.mutedText.Get());

    // [취소] Pill
    const D2D1_RECT_F rcCancel = GetCancelButtonRect();
    const D2D1_ROUNDED_RECT rrCancel = D2D1::RoundedRect(rcCancel, 17.0f, 17.0f);
    rt->FillRoundedRectangle(rrCancel,
        (m_pressedId == kIdBtnCancel) ? m_brushes.btnPressed.Get() : ((m_hoveredId == kIdBtnCancel) ? m_brushes.btnHover.Get() : m_brushes.btnBg.Get()));
    rt->DrawRoundedRectangle(rrCancel,
        (m_hoveredId == kIdBtnCancel) ? m_brushes.cyan.Get() : m_brushes.btnBorder.Get(), 1.0f);
    rt->DrawTextW(L"취소", 2, m_btnFormat.Get(), rcCancel,
        (m_hoveredId == kIdBtnCancel) ? m_brushes.text.Get() : m_brushes.subText.Get());

    // [저장] Glowing Cyan Pill
    const D2D1_RECT_F rcSave = GetSaveButtonRect();
    const D2D1_ROUNDED_RECT rrGlow = D2D1::RoundedRect(
        D2D1::RectF(rcSave.left - 2.0f, rcSave.top - 2.0f, rcSave.right + 2.0f, rcSave.bottom + 2.0f), 19.0f, 19.0f);
    rt->DrawRoundedRectangle(rrGlow, m_brushes.cyanGlow.Get(), 3.2f);
    rt->FillRoundedRectangle(D2D1::RoundedRect(rcSave, 17.0f, 17.0f), m_brushes.saveGrad.Get());
    rt->DrawTextW(L"저장", 2, m_btnBoldFormat.Get(), rcSave, m_brushes.darkText.Get());

    if (isPreview)
    {
        m_brushes.Reset();
    }
}

LRESULT CALLBACK SettingsView::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SettingsView* view = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        view = static_cast<SettingsView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        view->m_hWnd = hWnd;
    }
    else
    {
        view = reinterpret_cast<SettingsView*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (view)
    {
        return view->HandleMessage(hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsView::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto activeScrollPtr = [this]() -> float* {
        if (m_activeTab == 4) return m_showingApiGuide ? &m_apiGuideScroll : &m_apiSummaryScroll;
        if (m_activeTab == 5) return &m_infoScroll;
        return nullptr;
    };

    switch (msg)
    {
    case WM_NCCALCSIZE:
        return 0;

    case WM_NCPAINT:
        return 0;

    case WM_NCACTIVATE:
        return TRUE;

    case WM_TIMER:
        if (wParam == kTimerRefreshId)
        {
            if (m_activeTab == 4 && !m_showingApiGuide)
            {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (!m_footerFeedback.empty() && GetTickCount64() >= m_feedbackExpireTick)
            {
                m_footerFeedback.clear();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        break;

    case WM_COMMAND:
    {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == kIdApiIp || id == kIdApiPort)
        {
            if (code == EN_SETFOCUS || code == EN_KILLFOCUS)
            {
                const bool focused = (code == EN_SETFOCUS);
                if (id == kIdApiIp) m_ipFocused = focused;
                else m_portFocused = focused;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            else if (code == EN_CHANGE && m_activeTab == 4)
            {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(238, 242, 250));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(18, 22, 32));
        return reinterpret_cast<LRESULT>(m_editBackground ? m_editBackground : GetStockObject(BLACK_BRUSH));

    case WM_SIZE:
    {
        const UINT width = LOWORD(lParam);
        const UINT height = HIWORD(lParam);
        if (m_renderTarget && width > 0 && height > 0)
        {
            m_renderTarget->Resize(D2D1::SizeU(width, height));
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_DPICHANGED:
    {
        const UINT newDpi = HIWORD(wParam);
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested)
        {
            SetWindowPos(
                hWnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        m_currentDpi = newDpi ? newDpi : 96u;
        if (m_renderTarget)
        {
            const float fDpi = static_cast<float>(m_currentDpi);
            m_renderTarget->SetDpi(fDpi, fDpi);
        }
        RefreshEditFontForDpi(m_currentDpi);
        LayoutApiEditors();
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        BeginPaint(hWnd, &ps);
        Draw();
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        const float x = FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi);
        const float y = FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi);

        if (!m_trackingMouse)
        {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            m_trackingMouse = true;
        }

        if (m_isDraggingScroll)
        {
            if (float* scrollPtr = activeScrollPtr())
            {
                std::vector<BentoCard> cards;
                std::vector<CardLabel> labels;
                std::vector<ControlItem> controls;
                std::vector<FrameBox> frames;
                ScrollTextBox box;
                BuildActiveTabLayout(cards, labels, controls, frames, box);
                const float trackH = box.trackRect.bottom - box.trackRect.top;
                const float thumbH = box.thumbRect.bottom - box.thumbRect.top;
                const float travel = trackH - thumbH;
                if (travel > 0.5f && box.maxScroll > 0.5f)
                {
                    *scrollPtr = std::clamp(
                        m_dragStartScroll + ((y - m_dragStartY) / travel) * box.maxScroll,
                        0.0f, box.maxScroll);
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        const int hit = HitTestControl(x, y);
        if (hit != m_hoveredId)
        {
            m_hoveredId = hit;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_hoveredId != 0)
        {
            m_hoveredId = 0;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;

    case WM_MOUSEWHEEL:
    {
        if (float* scrollPtr = activeScrollPtr())
        {
            std::vector<BentoCard> cards;
            std::vector<CardLabel> labels;
            std::vector<ControlItem> controls;
            std::vector<FrameBox> frames;
            ScrollTextBox box;
            BuildActiveTabLayout(cards, labels, controls, frames, box);
            if (box.active && box.maxScroll > 0.5f)
            {
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                *scrollPtr = std::clamp(
                    *scrollPtr - static_cast<float>(delta) * 0.35f,
                    0.0f, box.maxScroll);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        break;
    }

    case WM_LBUTTONDOWN:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        const float x = FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi);
        const float y = FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi);

        const int hit = HitTestControl(x, y);
        if (hit == kIdScrollArea)
        {
            std::vector<BentoCard> cards;
            std::vector<CardLabel> labels;
            std::vector<ControlItem> controls;
            std::vector<FrameBox> frames;
            ScrollTextBox box;
            BuildActiveTabLayout(cards, labels, controls, frames, box);
            if (float* scrollPtr = activeScrollPtr(); scrollPtr && box.active && box.maxScroll > 0.5f)
            {
                if (y < box.thumbRect.top || y > box.thumbRect.bottom)
                {
                    const float thumbH = box.thumbRect.bottom - box.thumbRect.top;
                    const float travel = (std::max)(1.0f, (box.trackRect.bottom - box.trackRect.top) - thumbH);
                    const float rel = std::clamp(y - box.trackRect.top - thumbH * 0.5f, 0.0f, travel);
                    *scrollPtr = (rel / travel) * box.maxScroll;
                }
                m_isDraggingScroll = true;
                m_dragStartY = y;
                m_dragStartScroll = *scrollPtr;
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        if (hit > 0)
        {
            m_pressedId = hit;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        if (y < kHeaderHeight)
        {
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        const UINT dpi = FontHelper::GetWindowDpi(hWnd);
        const float x = FontHelper::PxToDip(GET_X_LPARAM(lParam), dpi);
        const float y = FontHelper::PxToDip(GET_Y_LPARAM(lParam), dpi);

        if (m_isDraggingScroll)
        {
            m_isDraggingScroll = false;
            ReleaseCapture();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        if (m_pressedId > 0)
        {
            const int pressed = m_pressedId;
            m_pressedId = 0;
            ReleaseCapture();
            if (HitTestControl(x, y) == pressed)
            {
                ExecuteControl(pressed);
            }
            else
            {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        CloseModal(false);
        return 0;

    case WM_DESTROY:
        DiscardDeviceResources();
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        m_hWnd = nullptr;
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
