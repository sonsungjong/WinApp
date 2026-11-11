#include "Views/SettingsView.h"
#include "Views/FontHelper.h"
#include "Resource.h"
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>

#pragma comment(lib, "dwmapi.lib")

namespace
{
    constexpr wchar_t kSettingsClassName[] = L"NextVideoPlayer_SettingsWnd";
    constexpr int kDlgWidth = 600;
    constexpr int kDlgHeight = 470;
    constexpr float kHeaderHeight = 58.0f;

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
}

SettingsView::SettingsView(const AppInfo& appInfo, HFONT dialogFont) : m_dialogFont(dialogFont), m_appInfo(appInfo)
{
    m_editBackground = CreateSolidBrush(RGB(18, 21, 30));
}

SettingsView::~SettingsView()
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        DestroyWindow(m_hWnd);
    }
    if (m_editBackground)
        DeleteObject(m_editBackground);
}

bool SettingsView::ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig)
{
    m_hInstance = hInstance;
    m_parentHwnd = parentHwnd;

    if (m_hWnd && IsWindow(m_hWnd))
    {
        SetForegroundWindow(m_hWnd);
        return false;
    }

    if (FAILED(CreateDeviceIndependentResources()))
    {
        return false;
    }

    if (!RegisterWindowClass(hInstance) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    m_initialConfig = initialConfig;
    m_currentConfig = initialConfig;
    m_accepted = false;
    m_done = false;
    m_pendingPostCommand = 0;
    m_hoveredId = -1;
    m_pressedId = -1;
    m_showingLicenses = false;

    RECT parentRc{};
    if (parentHwnd && !GetWindowRect(parentHwnd, &parentRc))
    {
        parentRc = RECT{ 100, 100, 100 + kDlgWidth, 100 + kDlgHeight };
    }

    int posX = parentRc.left + ((parentRc.right - parentRc.left) - kDlgWidth) / 2;
    int posY = parentRc.top + ((parentRc.bottom - parentRc.top) - kDlgHeight) / 2;

    const bool parentWasEnabled = parentHwnd ? (IsWindowEnabled(parentHwnd) != FALSE) : false;
    if (parentHwnd)
    {
        EnableWindow(parentHwnd, FALSE);
    }

    HWND hDlg =
        CreateWindowExW(0, kSettingsClassName, L"환경설정 (Preferences)", WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN, posX,
                        posY, kDlgWidth, kDlgHeight, parentHwnd, nullptr, hInstance, this);

    if (!hDlg)
    {
        if (parentHwnd && parentWasEnabled) EnableWindow(parentHwnd, TRUE);
        return false;
    }
    if (!CreateApiEditors() || !CreateInfoEditor())
    {
        DestroyWindow(hDlg);
        if (parentHwnd && parentWasEnabled)
            EnableWindow(parentHwnd, TRUE);
        return false;
    }
    UpdateTabVisibility();
    SetTimer(hDlg, 2, 250, nullptr);

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hDlg, 20, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hDlg, 19, &darkMode, sizeof(darkMode));
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hDlg, 33, &cornerPref, sizeof(cornerPref));

    MSG msg{};
    while (!m_done)
    {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0)
        {
            if (result == 0) PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }

        if (msg.message == WM_KEYDOWN)
        {
            if (msg.wParam == VK_ESCAPE)
            {
                if (m_onCancelClicked) m_onCancelClicked();
                else CloseModal(false);
                break;
            }
            if (msg.wParam == VK_RETURN)
            {
                if (m_onSaveClicked) m_onSaveClicked();
                else CloseModal(true);
                if (m_done)
                    break;
                continue;
            }
        }

        if (IsDialogMessageW(hDlg, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hDlg && IsWindow(hDlg))
    {
        DestroyWindow(hDlg);
    }

    if (parentHwnd && parentWasEnabled && IsWindow(parentHwnd))
    {
        EnableWindow(parentHwnd, TRUE);
        SetForegroundWindow(parentHwnd);
    }

    if (m_pendingPostCommand != 0 && parentHwnd && IsWindow(parentHwnd))
    {
        PostMessageW(parentHwnd, WM_COMMAND, MAKEWPARAM(m_pendingPostCommand, 0), 0);
        m_pendingPostCommand = 0;
    }

    return m_accepted;
}

AppConfig SettingsView::ReadFormValues(const AppConfig&) const
{
    return m_currentConfig;
}

bool SettingsView::CreateApiEditors()
{
    m_ipEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_currentConfig.api.ip.c_str(),
                               WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 180, 218, 360, 30, m_hWnd,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdApiIp)), m_hInstance, nullptr);
    m_portEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_currentConfig.api.port).c_str(),
                                 WS_CHILD | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 180, 260, 140, 30, m_hWnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdApiPort)), m_hInstance, nullptr);
    if (!m_ipEdit || !m_portEdit)
        return false;
    const auto font = m_dialogFont ? m_dialogFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    for (HWND edit : {m_ipEdit, m_portEdit})
        SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(m_ipEdit, EM_SETLIMITTEXT, 100, 0);
    SendMessageW(m_portEdit, EM_SETLIMITTEXT, 5, 0);
    return true;
}
void SettingsView::UpdateTabVisibility()
{
    for (HWND edit : {m_ipEdit, m_portEdit})
        if (edit)
            ShowWindow(edit, m_activeTab == 4 ? SW_SHOW : SW_HIDE);
    if (m_infoEdit)
        ShowWindow(m_infoEdit, m_activeTab == 5 ? SW_SHOW : SW_HIDE);
}

bool SettingsView::CreateInfoEditor()
{
    m_infoEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_appInfo.displayText.c_str(),
                        WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 26, 124, 548,
                        214, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdInfoText)), m_hInstance, nullptr);
    if (!m_infoEdit)
        return false;
    const auto font = m_dialogFont ? m_dialogFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(m_infoEdit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(m_infoEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(12, 12));
    SendMessageW(m_infoEdit, EM_SETLIMITTEXT, 1024 * 1024, 0);
    return true;
}

void SettingsView::ToggleLicenses()
{
    const bool showLicenses = !m_showingLicenses;
    const auto text = showLicenses ? AppInfo::LicenseText(m_hInstance) : m_appInfo.displayText;
    SetWindowTextW(m_infoEdit, text.c_str());
    SendMessageW(m_infoEdit, EM_SETSEL, 0, 0);
    SendMessageW(m_infoEdit, EM_SCROLLCARET, 0, 0);
    m_showingLicenses = showLicenses;
}

void SettingsView::OpenInfoLink(const std::wstring& target)
{
    const auto result =
        reinterpret_cast<INT_PTR>(ShellExecuteW(m_hWnd, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
        MessageBoxW(m_hWnd, L"연결된 프로그램을 열 수 없습니다. 정보 화면의 주소를 복사해 사용하세요.", L"앱 정보",
                    MB_OK | MB_ICONINFORMATION);
}
bool SettingsView::TryReadFormValues(const AppConfig&, AppConfig& result)
{
    auto config = m_currentConfig;
    wchar_t ip[128]{}, port[16]{};
    GetWindowTextW(m_ipEdit, ip, 128);
    GetWindowTextW(m_portEdit, port, 16);
    config.api.ip = ip;
    wchar_t* end = nullptr;
    const auto number = std::wcstol(port, &end, 10);
    if (!ApiSettings::IsValidIp(config.api.ip))
    {
        m_activeTab = 4;
        UpdateTabVisibility();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        MessageBoxW(m_hWnd, L"올바른 IP 주소를 입력하세요(127.0.0.1 등).", L"API 설정", MB_OK | MB_ICONINFORMATION);
        SetFocus(m_ipEdit);
        return false;
    }
    if (end == port || *end || number < 1 || number > 65535)
    {
        m_activeTab = 4;
        UpdateTabVisibility();
        InvalidateRect(m_hWnd, nullptr, FALSE);
        MessageBoxW(m_hWnd, L"포트는 1~65535 사이의 숫자로 입력하세요.", L"API 설정", MB_OK | MB_ICONINFORMATION);
        SetFocus(m_portEdit);
        return false;
    }
    config.api.port = static_cast<int>(number);
    result = std::move(config);
    return true;
}
void SettingsView::CopyApiAddress()
{
    AppConfig config;
    if (!TryReadFormValues(m_currentConfig, config))
        return;
    const auto text = L"ws" + config.api.HttpUrl().substr(4) + L"api/v1";
    if (!OpenClipboard(m_hWnd))
        return;
    const auto bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory)
    {
        if (auto* data = GlobalLock(memory))
        {
            std::memcpy(data, text.c_str(), bytes);
            GlobalUnlock(memory);
            EmptyClipboard();
            if (SetClipboardData(CF_UNICODETEXT, memory))
            {
                memory = nullptr;
                m_apiCopiedUntil = GetTickCount64() + 2500;
            }
        }
        if (memory)
            GlobalFree(memory);
    }
    CloseClipboard();
}

void SettingsView::CloseModal(bool accepted)
{
    m_accepted = accepted;
    m_done = true;
    if (m_hWnd && IsWindow(m_hWnd))
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void SettingsView::SetOnSaveClicked(std::function<void()> handler)
{
    m_onSaveClicked = std::move(handler);
}

void SettingsView::SetOnCancelClicked(std::function<void()> handler)
{
    m_onCancelClicked = std::move(handler);
}

ATOM SettingsView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsView::WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kSettingsClassName;
    return RegisterClassExW(&wc);
}

HRESULT SettingsView::CreateDeviceIndependentResources()
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
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        18.0f, L"ko-kr", m_titleBoldFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_titleBoldFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        15.0f, L"ko-kr", m_titleSubFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_titleSubFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0f, L"ko-kr", m_tabFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_tabFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_tabFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0f, L"ko-kr", m_tabActiveFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_tabActiveFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_tabActiveFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"ko-kr", m_cardTitleFormat.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.5f, L"ko-kr", m_rowLabelFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_rowLabelFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    hr = m_dwriteFactory->CreateTextFormat(
        korFontName, m_customFontCollection.Get(),
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.5f, L"ko-kr", m_valueFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_valueFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_valueFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

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
        13.5f, L"ko-kr", m_btnBoldFormat.GetAddressOf());
    if (FAILED(hr)) return hr;
    m_btnBoldFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_btnBoldFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    return S_OK;
}

HRESULT SettingsView::CreateDeviceResources()
{
    if (m_renderTarget) return S_OK;

    RECT rc{};
    GetClientRect(m_hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        std::max(1L, rc.right - rc.left),
        std::max(1L, rc.bottom - rc.top));

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hWnd, size),
        m_renderTarget.GetAddressOf());
    if (FAILED(hr)) return hr;

    if (FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x161922, 1.0f), m_bgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x12151E, 1.0f), m_footerBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x323848, 1.0f), m_windowBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x222736, 1.0f), m_tabActiveBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1E222E, 1.0f), m_cardBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x2B3142, 1.0f), m_cardBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x252B3B, 1.0f), m_separatorBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF2F5FA, 1.0f), m_textBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x949CB0, 1.0f), m_subTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x061A28, 1.0f), m_darkTextBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x00E1FF, 1.0f), m_cyanBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x00C8FF, 0.24f), m_cyanGlowBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x232836, 1.0f), m_toggleOffBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x4E566E, 1.0f), m_toggleOffBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x7E879E, 1.0f), m_toggleOffThumbBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x122230, 1.0f), m_toggleOnThumbBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x282E3E, 1.0f), m_btnBgBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x343C50, 1.0f), m_btnHoverBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x1C202B, 1.0f), m_btnPressedBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x3E465C, 1.0f), m_btnBorderBrush.GetAddressOf())) ||
        FAILED(hr = m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xC42B1C, 1.0f), m_closeHoverBrush.GetAddressOf())))
    {
        DiscardDeviceResources();
        return hr;
    }

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x00E5FF, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x00B4FF, 1.0f);
    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCol;
    if (SUCCEEDED(m_renderTarget->CreateGradientStopCollection(stops, 2, &stopCol)))
    {
        m_renderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(446.0f, 0.0f), D2D1::Point2F(574.0f, 0.0f)),
            stopCol.Get(),
            m_saveBtnGradBrush.GetAddressOf());
    }

    return S_OK;
}

void SettingsView::DiscardDeviceResources()
{
    m_saveBtnGradBrush.Reset();
    m_closeHoverBrush.Reset();
    m_btnBorderBrush.Reset();
    m_btnPressedBrush.Reset();
    m_btnHoverBrush.Reset();
    m_btnBgBrush.Reset();
    m_toggleOnThumbBrush.Reset();
    m_toggleOffThumbBrush.Reset();
    m_toggleOffBorderBrush.Reset();
    m_toggleOffBgBrush.Reset();
    m_cyanGlowBrush.Reset();
    m_cyanBrush.Reset();
    m_darkTextBrush.Reset();
    m_subTextBrush.Reset();
    m_textBrush.Reset();
    m_separatorBrush.Reset();
    m_cardBorderBrush.Reset();
    m_cardBgBrush.Reset();
    m_tabActiveBgBrush.Reset();
    m_windowBorderBrush.Reset();
    m_footerBgBrush.Reset();
    m_bgBrush.Reset();
    m_renderTarget.Reset();
}

D2D1_RECT_F SettingsView::GetCloseButtonRect() const
{
    return D2D1::RectF(540.0f, 16.0f, 574.0f, 50.0f);
}

D2D1_RECT_F SettingsView::GetTabRect(int tabIndex) const
{
    float startX = 26.0f;
    float tabW = (548.0f - 8.0f * (kTabCount - 1)) / kTabCount;
    float gap = 8.0f;
    float x = startX + tabIndex * (tabW + gap);
    return D2D1::RectF(x, 64.0f, x + tabW, 108.0f);
}

D2D1_RECT_F SettingsView::GetCancelButtonRect() const
{
    return D2D1::RectF(306.0f, 414.0f, 434.0f, 454.0f);
}

D2D1_RECT_F SettingsView::GetSaveButtonRect() const
{
    return D2D1::RectF(446.0f, 414.0f, 574.0f, 454.0f);
}

void SettingsView::BuildActiveTabLayout(
    std::vector<BentoCard>& outCards,
    std::vector<CardLabel>& outLabels,
    std::vector<ControlItem>& outControls) const
{
    outCards.clear();
    outLabels.clear();
    outControls.clear();

    wchar_t stepBuf[16]{};
    swprintf_s(stepBuf, L"%d초", m_currentConfig.seekStepSeconds);

    wchar_t scaleBuf[16]{};
    int pct = static_cast<int>(std::round(m_currentConfig.subtitleScale * 100.0));
    swprintf_s(scaleBuf, L"%d%%", pct);

    if (m_activeTab == 1) // 재생 (Playback) — 1:1 identical to view_settings.png!
    {
        // Left-Top Card: 방향키 탐색 시간
        outCards.push_back({ D2D1::RectF(26.0f, 124.0f, 290.0f, 236.0f), L"방향키 탐색 시간" });
        outControls.push_back({ kIdSeekMinus, D2D1::RectF(46.0f, 180.0f, 82.0f, 216.0f), L"−" });
        outControls.push_back({ kIdSeekDisplay, D2D1::RectF(82.0f, 180.0f, 138.0f, 216.0f), stepBuf, false, false, true });
        outControls.push_back({ kIdSeekPlus, D2D1::RectF(138.0f, 180.0f, 174.0f, 216.0f), L"+" });

        // Left-Bottom Card: 구간 반복 (A-B Repeat)
        outCards.push_back({ D2D1::RectF(26.0f, 250.0f, 290.0f, 362.0f), L"구간 반복 (A-B Repeat)" });
        outControls.push_back({ kIdABSetA, D2D1::RectF(46.0f, 304.0f, 124.0f, 340.0f), L"A 시작점" });
        outControls.push_back({ kIdABSetB, D2D1::RectF(132.0f, 304.0f, 210.0f, 340.0f), L"B 종료점" });
        outControls.push_back({ kIdABClear, D2D1::RectF(218.0f, 304.0f, 272.0f, 340.0f), L"해제" });

        // Right-Top Card: 반복 재생 (Repeat)
        outCards.push_back({ D2D1::RectF(306.0f, 124.0f, 574.0f, 196.0f), L"" });
        outLabels.push_back({ D2D1::RectF(326.0f, 142.0f, 490.0f, 178.0f), L"반복 재생 (Repeat)" });
        outControls.push_back({ kIdToggleRepeat, D2D1::RectF(506.0f, 148.0f, 554.0f, 172.0f), L"", true, m_currentConfig.repeatEnabled });

        // Right-Bottom Large Card: 자막 표시 / 폰트 크기 / 싱크 조절 / 창 항상 위
        outCards.push_back({ D2D1::RectF(306.0f, 210.0f, 574.0f, 384.0f), L"" });

        outLabels.push_back({ D2D1::RectF(326.0f, 224.0f, 480.0f, 254.0f), L"자막 표시" });
        outControls.push_back({ kIdToggleSub, D2D1::RectF(506.0f, 227.0f, 554.0f, 251.0f), L"", true, m_currentConfig.subtitleVisible });

        outLabels.push_back({ D2D1::RectF(326.0f, 264.0f, 415.0f, 298.0f), L"폰트 크기" });
        outControls.push_back({ kIdSubScaleMinus, D2D1::RectF(420.0f, 264.0f, 454.0f, 298.0f), L"−" });
        outControls.push_back({ kIdSubScaleDisplay, D2D1::RectF(454.0f, 264.0f, 520.0f, 298.0f), scaleBuf, false, false, true });
        outControls.push_back({ kIdSubScalePlus, D2D1::RectF(520.0f, 264.0f, 554.0f, 298.0f), L"+" });

        outLabels.push_back({ D2D1::RectF(326.0f, 308.0f, 390.0f, 342.0f), L"싱크 지연" });
        outControls.push_back({ kIdSubDelayMinus, D2D1::RectF(392.0f, 308.0f, 468.0f, 342.0f), L"[-0.5초]" });
        outControls.push_back({ kIdSubDelayPlus, D2D1::RectF(476.0f, 308.0f, 554.0f, 342.0f), L"[+0.5초]" });

        outLabels.push_back({ D2D1::RectF(326.0f, 348.0f, 480.0f, 378.0f), L"창 항상 위" });
        outControls.push_back({ kIdToggleTopMost, D2D1::RectF(506.0f, 351.0f, 554.0f, 375.0f), L"", true, m_currentConfig.topMost });
    }
    else if (m_activeTab == 0) // 파일 (File)
    {
        outCards.push_back({ D2D1::RectF(26.0f, 124.0f, 290.0f, 236.0f), L"동영상 파일 열기" });
        outControls.push_back({ kIdFileOpen, D2D1::RectF(46.0f, 180.0f, 200.0f, 216.0f), L"동영상 찾아보기..." });

        outCards.push_back({ D2D1::RectF(26.0f, 250.0f, 290.0f, 362.0f), L"외부 자막 파일 열기" });
        outControls.push_back({ kIdSubOpen, D2D1::RectF(46.0f, 304.0f, 200.0f, 340.0f), L"자막 파일 열기..." });

        outCards.push_back({ D2D1::RectF(306.0f, 124.0f, 574.0f, 236.0f), L"현재 화면 캡처 (Screenshot)" });
        outControls.push_back({ kIdScreenshot, D2D1::RectF(326.0f, 180.0f, 480.0f, 216.0f), L"화면 캡처 저장..." });

        outCards.push_back({ D2D1::RectF(306.0f, 250.0f, 574.0f, 362.0f), L"재생 제어 및 종료" });
        outControls.push_back({ kIdPlayPause, D2D1::RectF(326.0f, 304.0f, 412.0f, 340.0f), L"재생/일시" });
        outControls.push_back({ kIdStop, D2D1::RectF(420.0f, 304.0f, 480.0f, 340.0f), L"정지" });
        outControls.push_back({ kIdExitApp, D2D1::RectF(488.0f, 304.0f, 554.0f, 340.0f), L"끝내기" });
    }
    else if (m_activeTab == 2) // 자막 (Subtitle)
    {
        outCards.push_back({ D2D1::RectF(26.0f, 124.0f, 290.0f, 236.0f), L"자막 표시 및 불러오기" });
        outLabels.push_back({ D2D1::RectF(46.0f, 180.0f, 130.0f, 216.0f), L"자막 표시" });
        outControls.push_back({ kIdToggleSub, D2D1::RectF(126.0f, 186.0f, 174.0f, 210.0f), L"", true, m_currentConfig.subtitleVisible });
        outControls.push_back({ kIdSubOpen, D2D1::RectF(186.0f, 180.0f, 272.0f, 216.0f), L"자막 열기" });

        outCards.push_back({ D2D1::RectF(26.0f, 250.0f, 290.0f, 362.0f), L"자막 상하 위치 (Position)" });
        outControls.push_back({ kIdSubPosUp, D2D1::RectF(46.0f, 304.0f, 152.0f, 340.0f), L"▲ 위치 위로" });
        outControls.push_back({ kIdSubPosDown, D2D1::RectF(162.0f, 304.0f, 272.0f, 340.0f), L"▼ 위치 아래로" });

        outCards.push_back({ D2D1::RectF(306.0f, 124.0f, 574.0f, 236.0f), L"폰트 크기 조절 (Scale)" });
        outControls.push_back({ kIdSubScaleMinus, D2D1::RectF(326.0f, 180.0f, 362.0f, 216.0f), L"−" });
        outControls.push_back({ kIdSubScaleDisplay, D2D1::RectF(366.0f, 180.0f, 436.0f, 216.0f), scaleBuf, false, false, true });
        outControls.push_back({ kIdSubScalePlus, D2D1::RectF(440.0f, 180.0f, 476.0f, 216.0f), L"+" });

        outCards.push_back({ D2D1::RectF(306.0f, 250.0f, 574.0f, 362.0f), L"자막 싱크 조절 (Delay)" });
        outControls.push_back({ kIdSubDelayMinus, D2D1::RectF(326.0f, 304.0f, 434.0f, 340.0f), L"[-0.5초 빠름]" });
        outControls.push_back({ kIdSubDelayPlus, D2D1::RectF(444.0f, 304.0f, 554.0f, 340.0f), L"[+0.5초 느림]" });
    }
    else if (m_activeTab == 3) // 보기 (View)
    {
        outCards.push_back({ D2D1::RectF(26.0f, 124.0f, 290.0f, 236.0f), L"창 항상 위 (Always on Top)" });
        outLabels.push_back({ D2D1::RectF(46.0f, 180.0f, 190.0f, 216.0f), L"항상 맨 위에 고정" });
        outControls.push_back({ kIdToggleTopMost, D2D1::RectF(224.0f, 186.0f, 272.0f, 210.0f), L"", true, m_currentConfig.topMost });

        outCards.push_back({ D2D1::RectF(26.0f, 250.0f, 290.0f, 362.0f), L"전체화면 모드 (Fullscreen)" });
        outControls.push_back({ kIdFullscreen, D2D1::RectF(46.0f, 304.0f, 210.0f, 340.0f), L"전체화면 전환 (Enter)" });

        outCards.push_back({ D2D1::RectF(306.0f, 124.0f, 574.0f, 236.0f), L"재생목록 창 (Playlist)" });
        outControls.push_back({ kIdPlaylist, D2D1::RectF(326.0f, 180.0f, 490.0f, 216.0f), L"재생목록 열기 / 닫기" });

        outCards.push_back({ D2D1::RectF(306.0f, 250.0f, 574.0f, 362.0f), L"이전 / 다음 미디어 이동" });
        outControls.push_back({ kIdPrevFile, D2D1::RectF(326.0f, 304.0f, 434.0f, 340.0f), L"⏮ 이전 파일" });
        outControls.push_back({ kIdNextFile, D2D1::RectF(444.0f, 304.0f, 554.0f, 340.0f), L"⏭ 다음 파일" });
    }
    else if (m_activeTab == 4)
    {
        outCards.push_back({D2D1::RectF(26, 124, 574, 384), L"WebSocket API"});
        outLabels.push_back({D2D1::RectF(46, 167, 480, 199), L"외부 프로그램으로 플레이어 제어"});
        outControls.push_back({kIdApiEnable, D2D1::RectF(506, 171, 554, 195), L"", true, m_currentConfig.api.enabled});
        outLabels.push_back({D2D1::RectF(46, 217, 170, 249), L"수신 IP"});
        outLabels.push_back({D2D1::RectF(46, 259, 170, 291), L"포트"});
        outControls.push_back({kIdApiPage, D2D1::RectF(338, 258, 554, 296), L"켜고 제어화면 열기"});
        const auto status = GetTickCount64() < m_apiCopiedUntil ? L"WebSocket 주소를 복사했습니다."
                                                                : (m_apiStatus ? m_apiStatus() : L"API 꺼짐");
        outLabels.push_back({D2D1::RectF(46, 302, 554, 335), status});
        outControls.push_back({kIdApiCopy, D2D1::RectF(46, 342, 262, 376), L"WebSocket 주소 복사"});
        outLabels.push_back({D2D1::RectF(278, 342, 554, 376), L"기본값은 같은 PC에서 접속"});
    }
    else if (m_activeTab == 5)
    {
        outControls.push_back({kIdInfoEmail, D2D1::RectF(26, 350, 202, 384), L"이메일 문의"});
        outControls.push_back({kIdInfoYoutube, D2D1::RectF(212, 350, 388, 384),
                               m_appInfo.youtubeUrl.empty() ? L"YouTube 주소 준비 중" : L"YouTube 채널", false, false,
                               m_appInfo.youtubeUrl.empty()});
        outControls.push_back({kIdInfoLicense, D2D1::RectF(398, 350, 574, 384),
                               m_showingLicenses ? L"앱 정보로 돌아가기" : L"라이선스 전문"});
    }
}

int SettingsView::HitTest(float x, float y) const
{
    auto ptIn = [](const D2D1_RECT_F& rc, float px, float py) {
        return px >= rc.left && px <= rc.right && py >= rc.top && py <= rc.bottom;
    };

    if (ptIn(GetCloseButtonRect(), x, y)) return kIdCloseBtn;

    for (int i = 0; i < kTabCount; ++i)
    {
        if (ptIn(GetTabRect(i), x, y)) return kIdTabFile + i;
    }

    std::vector<BentoCard> cards;
    std::vector<CardLabel> labels;
    std::vector<ControlItem> controls;
    BuildActiveTabLayout(cards, labels, controls);

    for (const auto& ctrl : controls)
    {
        if (!ctrl.isDisplayOnly && ptIn(ctrl.rect, x, y))
        {
            return ctrl.id;
        }
    }

    if (ptIn(GetSaveButtonRect(), x, y)) return kIdBtnSave;
    if (ptIn(GetCancelButtonRect(), x, y)) return kIdBtnCancel;

    return -1;
}

void SettingsView::DispatchParentCommand(int commandId, bool closeAfter)
{
    if (closeAfter)
    {
        m_pendingPostCommand = commandId;
        if (m_onSaveClicked) m_onSaveClicked();
        else CloseModal(true);
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
        UpdateTabVisibility();
        break;
    case kIdApiEnable:
        m_currentConfig.api.enabled = !m_currentConfig.api.enabled;
        break;
    case kIdApiPage:
        m_currentConfig.api.enabled = true;
        if (m_onApiPage)
            m_onApiPage();
        break;
    case kIdApiCopy:
        CopyApiAddress();
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
        m_currentConfig.seekStepSeconds = std::max(1, m_currentConfig.seekStepSeconds - 1);
        break;
    case kIdSeekPlus:
        m_currentConfig.seekStepSeconds = std::min(60, m_currentConfig.seekStepSeconds + 1);
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
        DispatchParentCommand(IDM_VIEW_PLAYLIST);
        break;
    }
}

void SettingsView::DrawGearIcon(float cx, float cy, float radius)
{
    m_renderTarget->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.72f, radius * 0.72f),
        m_subTextBrush.Get(),
        1.8f);
    m_renderTarget->DrawEllipse(
        D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.34f, radius * 0.34f),
        m_subTextBrush.Get(),
        1.5f);

    for (int i = 0; i < 8; ++i)
    {
        float angle = i * (3.14159265f * 0.25f);
        float c = std::cos(angle);
        float s = std::sin(angle);
        m_renderTarget->DrawLine(
            D2D1::Point2F(cx + c * (radius * 0.70f), cy + s * (radius * 0.70f)),
            D2D1::Point2F(cx + c * radius, cy + s * radius),
            m_subTextBrush.Get(),
            2.4f);
    }
}

void SettingsView::DrawToggleSwitch(const D2D1_RECT_F& rect, bool state)
{
    D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(rect, 12.0f, 12.0f);
    float cy = (rect.top + rect.bottom) * 0.5f;

    if (state)
    {
        // Soft neon cyan outer glow + bright cyan pill + dark inner thumb circle (just like Image 2!)
        D2D1_ROUNDED_RECT glowPill = D2D1::RoundedRect(
            D2D1::RectF(rect.left - 1.5f, rect.top - 1.5f, rect.right + 1.5f, rect.bottom + 1.5f),
            13.5f, 13.5f);
        m_renderTarget->DrawRoundedRectangle(glowPill, m_cyanGlowBrush.Get(), 2.5f);
        m_renderTarget->FillRoundedRectangle(pill, m_cyanBrush.Get());

        float cx = rect.right - 12.0f;
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 7.5f, 7.5f), m_toggleOnThumbBrush.Get());
    }
    else
    {
        // Dark pill with subtle border + grey thumb on left (just like bottom toggle in Image 2!)
        m_renderTarget->FillRoundedRectangle(pill, m_toggleOffBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(pill, m_toggleOffBorderBrush.Get(), 1.2f);

        float cx = rect.left + 12.0f;
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 7.0f, 7.0f), m_toggleOffThumbBrush.Get());
    }
}

void SettingsView::Draw()
{
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    if (FAILED(CreateDeviceResources())) return;

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0x161922, 1.0f));

    D2D1_SIZE_F rtSize = m_renderTarget->GetSize();

    // 1. Custom Header (Gear icon + 환경설정 (Preferences) + [✕] button)
    DrawGearIcon(38.0f, 34.0f, 11.5f);

    m_renderTarget->DrawTextW(
        L"환경설정",
        4,
        m_titleBoldFormat.Get(),
        D2D1::RectF(58.0f, 14.0f, 140.0f, 52.0f),
        m_textBrush.Get());

    m_renderTarget->DrawTextW(
        L"(Preferences)",
        13,
        m_titleSubFormat.Get(),
        D2D1::RectF(138.0f, 15.0f, 280.0f, 52.0f),
        m_subTextBrush.Get());

    // Close button [✕]
    {
        D2D1_RECT_F rcClose = GetCloseButtonRect();
        bool hover = (m_hoveredId == kIdCloseBtn);
        D2D1_ROUNDED_RECT rClose = D2D1::RoundedRect(rcClose, 7.0f, 7.0f);
        m_renderTarget->FillRoundedRectangle(rClose, hover ? m_closeHoverBrush.Get() : m_tabActiveBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(rClose, hover ? m_closeHoverBrush.Get() : m_cardBorderBrush.Get(), 1.0f);

        float cx = (rcClose.left + rcClose.right) * 0.5f;
        float cy = (rcClose.top + rcClose.bottom) * 0.5f;
        ID2D1SolidColorBrush* xBrush = hover ? m_textBrush.Get() : m_subTextBrush.Get();
        m_renderTarget->DrawLine(D2D1::Point2F(cx - 5.0f, cy - 5.0f), D2D1::Point2F(cx + 5.0f, cy + 5.0f), xBrush, 1.5f);
        m_renderTarget->DrawLine(D2D1::Point2F(cx + 5.0f, cy - 5.0f), D2D1::Point2F(cx - 5.0f, cy + 5.0f), xBrush, 1.5f);
    }

    // 2. 4-Tab Bar
    m_renderTarget->DrawLine(
        D2D1::Point2F(0.0f, 108.0f),
        D2D1::Point2F(rtSize.width, 108.0f),
        m_separatorBrush.Get(),
        1.0f);

    const wchar_t* tabNames[kTabCount] = {L"파일", L"재생", L"자막", L"보기", L"API", L"정보"};

    for (int i = 0; i < kTabCount; ++i)
    {
        D2D1_RECT_F rcTab = GetTabRect(i);
        bool isActive = (m_activeTab == i);
        bool isHover = (m_hoveredId == kIdTabFile + i);

        if (isActive)
        {
            D2D1_ROUNDED_RECT rTab = D2D1::RoundedRect(rcTab, 8.0f, 8.0f);
            m_renderTarget->FillRoundedRectangle(rTab, m_tabActiveBgBrush.Get());

            // Glowing neon cyan underline (just like Image 2!)
            D2D1_ROUNDED_RECT glowBar = D2D1::RoundedRect(
                D2D1::RectF(rcTab.left + 6.0f, rcTab.bottom - 4.0f, rcTab.right - 6.0f, rcTab.bottom + 2.0f),
                3.0f, 3.0f);
            m_renderTarget->FillRoundedRectangle(glowBar, m_cyanGlowBrush.Get());

            D2D1_ROUNDED_RECT coreBar = D2D1::RoundedRect(
                D2D1::RectF(rcTab.left + 10.0f, rcTab.bottom - 2.5f, rcTab.right - 10.0f, rcTab.bottom + 0.5f),
                1.5f, 1.5f);
            m_renderTarget->FillRoundedRectangle(coreBar, m_cyanBrush.Get());
        }
        else if (isHover)
        {
            D2D1_ROUNDED_RECT rTab = D2D1::RoundedRect(rcTab, 8.0f, 8.0f);
            m_renderTarget->FillRoundedRectangle(rTab, m_cardBgBrush.Get());
        }

        m_renderTarget->DrawTextW(
            tabNames[i],
            static_cast<UINT32>(wcslen(tabNames[i])),
            isActive ? m_tabActiveFormat.Get() : m_tabFormat.Get(),
            rcTab,
            (isActive || isHover) ? m_textBrush.Get() : m_subTextBrush.Get());
    }

    // 3. 2-Column Bento Cards & Controls
    std::vector<BentoCard> cards;
    std::vector<CardLabel> labels;
    std::vector<ControlItem> controls;
    BuildActiveTabLayout(cards, labels, controls);

    for (const auto& card : cards)
    {
        D2D1_ROUNDED_RECT rCard = D2D1::RoundedRect(card.rect, 10.0f, 10.0f);
        m_renderTarget->FillRoundedRectangle(rCard, m_cardBgBrush.Get());
        m_renderTarget->DrawRoundedRectangle(rCard, m_cardBorderBrush.Get(), 1.0f);

        if (!card.title.empty())
        {
            m_renderTarget->DrawTextW(
                card.title.c_str(),
                static_cast<UINT32>(card.title.length()),
                m_cardTitleFormat.Get(),
                D2D1::RectF(card.rect.left + 20.0f, card.rect.top + 18.0f, card.rect.right - 16.0f, card.rect.top + 44.0f),
                m_textBrush.Get());
        }
    }

    for (const auto& lbl : labels)
    {
        m_renderTarget->DrawTextW(
            lbl.text.c_str(),
            static_cast<UINT32>(lbl.text.length()),
            m_rowLabelFormat.Get(),
            lbl.rect,
            m_textBrush.Get());
    }

    for (const auto& ctrl : controls)
    {
        if (ctrl.isToggle)
        {
            DrawToggleSwitch(ctrl.rect, ctrl.toggleState);
        }
        else if (ctrl.isDisplayOnly)
        {
            m_renderTarget->DrawTextW(
                ctrl.label.c_str(),
                static_cast<UINT32>(ctrl.label.length()),
                m_valueFormat.Get(),
                ctrl.rect,
                m_textBrush.Get());
        }
        else
        {
            bool hover = (m_hoveredId == ctrl.id);
            bool press = (m_pressedId == ctrl.id);
            D2D1_ROUNDED_RECT rBtn = D2D1::RoundedRect(ctrl.rect, 6.0f, 6.0f);

            m_renderTarget->FillRoundedRectangle(
                rBtn,
                press ? m_btnPressedBrush.Get() : (hover ? m_btnHoverBrush.Get() : m_btnBgBrush.Get()));
            m_renderTarget->DrawRoundedRectangle(
                rBtn,
                hover ? m_cyanBrush.Get() : m_btnBorderBrush.Get(),
                1.0f);
            m_renderTarget->DrawTextW(
                ctrl.label.c_str(),
                static_cast<UINT32>(ctrl.label.length()),
                m_btnFormat.Get(),
                ctrl.rect,
                m_textBrush.Get());
        }
    }

    // 4. Footer Bar
    D2D1_RECT_F rcFooter = D2D1::RectF(1.0f, 398.0f, rtSize.width - 1.0f, rtSize.height - 1.0f);
    m_renderTarget->FillRectangle(rcFooter, m_footerBgBrush.Get());
    m_renderTarget->DrawLine(
        D2D1::Point2F(0.0f, 398.0f),
        D2D1::Point2F(rtSize.width, 398.0f),
        m_separatorBrush.Get(),
        1.0f);

    // Cancel Button [취소]
    {
        D2D1_RECT_F rcCancel = GetCancelButtonRect();
        D2D1_ROUNDED_RECT rCancel = D2D1::RoundedRect(rcCancel, 8.0f, 8.0f);
        bool hover = (m_hoveredId == kIdBtnCancel);
        bool press = (m_pressedId == kIdBtnCancel);

        m_renderTarget->FillRoundedRectangle(
            rCancel,
            press ? m_btnPressedBrush.Get() : (hover ? m_btnHoverBrush.Get() : m_cardBgBrush.Get()));
        m_renderTarget->DrawRoundedRectangle(rCancel, m_btnBorderBrush.Get(), 1.0f);
        m_renderTarget->DrawTextW(L"취소", 2, m_btnFormat.Get(), rcCancel, m_textBrush.Get());
    }

    // Save Button [저장] with Neon Cyan Glow (just like Image 2!)
    {
        D2D1_RECT_F rcSave = GetSaveButtonRect();
        D2D1_ROUNDED_RECT rSave = D2D1::RoundedRect(rcSave, 8.0f, 8.0f);
        D2D1_ROUNDED_RECT glowSave = D2D1::RoundedRect(
            D2D1::RectF(rcSave.left - 2.0f, rcSave.top - 2.0f, rcSave.right + 2.0f, rcSave.bottom + 2.0f),
            10.0f, 10.0f);
        m_renderTarget->DrawRoundedRectangle(glowSave, m_cyanGlowBrush.Get(), 4.0f);

        if (m_saveBtnGradBrush)
        {
            m_renderTarget->FillRoundedRectangle(rSave, m_saveBtnGradBrush.Get());
        }
        else
        {
            m_renderTarget->FillRoundedRectangle(rSave, m_cyanBrush.Get());
        }
        m_renderTarget->DrawTextW(L"저장", 2, m_btnBoldFormat.Get(), rcSave, m_darkTextBrush.Get());
    }

    // Outer 1.2px window border
    m_renderTarget->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.6f, 0.6f, rtSize.width - 0.6f, rtSize.height - 0.6f), 8.0f, 8.0f),
        m_windowBorderBrush.Get(),
        1.2f);

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
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

    return view ? view->HandleMessage(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsView::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TIMER:
        if (m_activeTab == 4)
            InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(237, 241, 250));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(18, 21, 30));
        return reinterpret_cast<LRESULT>(m_editBackground ? m_editBackground : GetStockObject(BLACK_BRUSH));
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
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        if (!m_trackingMouse)
        {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            m_trackingMouse = true;
        }

        int hit = HitTest(x, y);
        if (hit != m_hoveredId)
        {
            m_hoveredId = hit;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
    {
        m_trackingMouse = false;
        m_hoveredId = -1;
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        int hit = HitTest(x, y);
        if (hit >= 0)
        {
            m_pressedId = hit;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        // Drag frameless modal window via top header
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
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));

        if (m_pressedId >= 0)
        {
            int executed = m_pressedId;
            m_pressedId = -1;
            ReleaseCapture();

            if (HitTest(x, y) == executed)
            {
                ExecuteControl(executed);
                if (m_done || !m_hWnd || !IsWindow(m_hWnd))
                {
                    return 0;
                }
            }
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CLOSE:
        if (m_onCancelClicked) m_onCancelClicked();
        else CloseModal(false);
        return 0;

    case WM_NCDESTROY:
        m_ipEdit = nullptr;
        m_portEdit = nullptr;
        m_infoEdit = nullptr;
        DiscardDeviceResources();
        m_done = true;
        m_hWnd = nullptr;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
