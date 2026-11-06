#include "Views/SettingsView.h"
#include "Views/FontHelper.h"
#include <algorithm>

namespace
{
    constexpr wchar_t kSettingsClassName[] = L"NextVideoPlayer_SettingsWnd";

    constexpr int IDC_SET_SEEK_EDIT = 1001;
    constexpr int IDC_SET_REPEAT_CHK = 1002;
    constexpr int IDC_SET_TOPMOST_CHK = 1003;
    constexpr int IDC_SET_SUB_CHK = 1004;
    constexpr int IDC_SET_OK = 1005;
    constexpr int IDC_SET_CANCEL = 1006;
}

SettingsView::~SettingsView()
{
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}

bool SettingsView::ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig)
{
    if (m_hWnd && IsWindow(m_hWnd))
    {
        SetForegroundWindow(m_hWnd);
        return false;
    }
    if (!RegisterWindowClass(hInstance) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    m_initialConfig = initialConfig;
    m_accepted = false;
    m_done = false;
    m_hWnd = nullptr;
    m_hSeekEdit = nullptr;
    m_hRepeatChk = nullptr;
    m_hTopMostChk = nullptr;
    m_hSubChk = nullptr;

    RECT parentRc{};
    if (!GetWindowRect(parentHwnd, &parentRc)) return false;
    int dlgW = 320;
    int dlgH = 240;
    int posX = parentRc.left + ((parentRc.right - parentRc.left) - dlgW) / 2;
    int posY = parentRc.top + ((parentRc.bottom - parentRc.top) - dlgH) / 2;

    const bool parentWasEnabled = IsWindowEnabled(parentHwnd) != FALSE;
    EnableWindow(parentHwnd, FALSE);

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSettingsClassName,
        L"환경설정 (config.ini)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX,
        posY,
        dlgW,
        dlgH,
        parentHwnd,
        nullptr,
        hInstance,
        this);

    if (!hDlg)
    {
        if (parentWasEnabled) EnableWindow(parentHwnd, TRUE);
        return false;
    }

    MSG msg{};
    while (!m_done)
    {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0)
        {
            if (result == 0) PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
        {
            if (m_onCancelClicked)
            {
                m_onCancelClicked();
            }
            else
            {
                CloseModal(false);
            }
            break;
        }
        if (!IsDialogMessageW(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hDlg && IsWindow(hDlg))
    {
        DestroyWindow(hDlg);
    }

    if (parentWasEnabled && IsWindow(parentHwnd))
    {
        EnableWindow(parentHwnd, TRUE);
        SetForegroundWindow(parentHwnd);
    }

    return m_accepted;
}

AppConfig SettingsView::ReadFormValues(const AppConfig& baseConfig) const
{
    AppConfig result = baseConfig;
    if (m_hSeekEdit && IsWindow(m_hSeekEdit))
    {
        WCHAR buf[32]{};
        GetWindowTextW(m_hSeekEdit, buf, 32);
        int step = _wtoi(buf);
        result.seekStepSeconds = std::clamp(step > 0 ? step : 5, 1, 600);
    }
    if (m_hRepeatChk && IsWindow(m_hRepeatChk))
    {
        result.repeatEnabled = (SendMessageW(m_hRepeatChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (m_hTopMostChk && IsWindow(m_hTopMostChk))
    {
        result.topMost = (SendMessageW(m_hTopMostChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    if (m_hSubChk && IsWindow(m_hSubChk))
    {
        result.subtitleVisible = (SendMessageW(m_hSubChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }
    return result;
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
    WNDCLASSEXW settingsWc{};
    settingsWc.cbSize = sizeof(WNDCLASSEXW);
    settingsWc.lpfnWndProc = SettingsView::WindowProc;
    settingsWc.hInstance = hInstance;
    settingsWc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsWc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    settingsWc.lpszClassName = kSettingsClassName;
    return RegisterClassExW(&settingsWc);
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
    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT hFont = m_dialogFont;

        HWND hLblSeek = CreateWindowExW(0, L"STATIC", L"좌/우 방향키 이동 시간 (초):",
            WS_CHILD | WS_VISIBLE, 20, 22, 180, 20, hWnd, nullptr, nullptr, nullptr);
        SendMessageW(hLblSeek, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        std::wstring seekStr = std::to_wstring(m_initialConfig.seekStepSeconds);
        m_hSeekEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", seekStr.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 205, 19, 80, 24,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_SEEK_EDIT)), nullptr, nullptr);
        SendMessageW(m_hSeekEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        m_hRepeatChk = CreateWindowExW(0, L"BUTTON", L"반복 재생 사용 (Repeat)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 20, 58, 260, 22,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_REPEAT_CHK)), nullptr, nullptr);
        SendMessageW(m_hRepeatChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessageW(m_hRepeatChk, BM_SETCHECK, m_initialConfig.repeatEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

        m_hTopMostChk = CreateWindowExW(0, L"BUTTON", L"창 항상 위 고정 (Always on Top)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 20, 88, 260, 22,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_TOPMOST_CHK)), nullptr, nullptr);
        SendMessageW(m_hTopMostChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessageW(m_hTopMostChk, BM_SETCHECK, m_initialConfig.topMost ? BST_CHECKED : BST_UNCHECKED, 0);

        m_hSubChk = CreateWindowExW(0, L"BUTTON", L"자막 표시 (Subtitle Visible)",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 20, 118, 260, 22,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_SUB_CHK)), nullptr, nullptr);
        SendMessageW(m_hSubChk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        SendMessageW(m_hSubChk, BM_SETCHECK, m_initialConfig.subtitleVisible ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND hBtnOk = CreateWindowExW(0, L"BUTTON", L"저장",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 110, 160, 80, 28,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_OK)), nullptr, nullptr);
        SendMessageW(hBtnOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        HWND hBtnCancel = CreateWindowExW(0, L"BUTTON", L"취소",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 205, 160, 80, 28,
            hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SET_CANCEL)), nullptr, nullptr);
        SendMessageW(hBtnCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDC_SET_OK)
        {
            if (m_onSaveClicked)
            {
                m_onSaveClicked();
            }
            return 0;
        }
        if (id == IDC_SET_CANCEL)
        {
            if (m_onCancelClicked)
            {
                m_onCancelClicked();
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (m_onCancelClicked)
        {
            m_onCancelClicked();
        }
        else
        {
            CloseModal(false);
        }
        return 0;
    case WM_NCDESTROY:
        m_done = true;
        m_hWnd = nullptr;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
