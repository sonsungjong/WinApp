#include "Views/SettingView.h"
#include "Resource.h"
#include <utility>

SettingView::SettingView(HINSTANCE instance, HWND owner)
    : m_instance(instance), m_owner(owner)
{
}

SettingView::~SettingView()
{
    m_onCloseRequested = {};
    Close();
}

void SettingView::SetTitle(const std::wstring& title)
{
    m_title = title;
    if (m_dialog)
        SetWindowTextW(m_dialog, m_title.c_str());
}

void SettingView::ShowModal()
{
    if (m_dialog)
    {
        SetForegroundWindow(m_dialog);
        return;
    }

    const INT_PTR result = DialogBoxParamW(m_instance, MAKEINTRESOURCEW(IDD_SETTINGBOX),
        m_owner, DialogProc, reinterpret_cast<LPARAM>(this));
    m_dialog = nullptr;
    if (result == -1)
        MessageBoxW(m_owner, L"설정 창을 열 수 없습니다.", m_title.c_str(), MB_OK | MB_ICONERROR);
}

void SettingView::Close()
{
    if (m_dialog)
        EndDialog(m_dialog, IDCANCEL);
}

void SettingView::SetOnCloseRequested(std::function<void()> handler)
{
    m_onCloseRequested = std::move(handler);
}

void SettingView::RequestClose()
{
    if (m_onCloseRequested)
        m_onCloseRequested();
    else
        Close();
}

INT_PTR CALLBACK SettingView::DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* view = reinterpret_cast<SettingView*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG)
    {
        view = reinterpret_cast<SettingView*>(lParam);
        view->m_dialog = dialog;
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(view));
    }
    return view ? view->HandleMessage(message, wParam) : FALSE;
}

INT_PTR SettingView::HandleMessage(UINT message, WPARAM wParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SetWindowTextW(m_dialog, m_title.c_str());
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            RequestClose();
            return TRUE;
        }
        return FALSE;
    case WM_CLOSE:
        RequestClose();
        return TRUE;
    case WM_NCDESTROY:
        SetWindowLongPtrW(m_dialog, DWLP_USER, 0);
        m_dialog = nullptr;
        return FALSE;
    default:
        return FALSE;
    }
}
