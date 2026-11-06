#include "Views/PlaylistView.h"
#include "Views/FileDialogs.h"
#include <algorithm>
#include <filesystem>

namespace
{
    constexpr wchar_t kClassName[] = L"NextVideoPlayer_PlaylistWnd";
    constexpr int kList = 2001, kPlay = 2002, kAdd = 2003, kRemove = 2004, kClear = 2005;
}

PlaylistView::~PlaylistView()
{
    if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
}

bool PlaylistView::Initialize(HINSTANCE instance, HWND owner)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    RECT rc{};
    if (!GetWindowRect(owner, &rc)) return false;
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"재생목록 (Playlist)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, rc.right - 360, rc.top + 60,
        340, 420, owner, nullptr, instance, this);
    return m_hwnd != nullptr;
}

bool PlaylistView::IsVisible() const
{
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void PlaylistView::SetVisible(bool visible)
{
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    if (m_onVisibilityChanged) m_onVisibilityChanged();
}

void PlaylistView::Render(const std::vector<std::wstring>& items, int currentIndex)
{
    if (!m_list || !IsWindow(m_list)) return;
    std::vector<std::wstring> selectedFiles;
    for (int index : GetSelectedIndices())
        if (index >= 0 && index < static_cast<int>(m_items.size())) selectedFiles.push_back(m_items[index]);
    int caret = static_cast<int>(SendMessageW(m_list, LB_GETCARETINDEX, 0, 0));
    const std::wstring caretFile = caret >= 0 && caret < static_cast<int>(m_items.size()) ? m_items[caret] : L"";
    int top = static_cast<int>(SendMessageW(m_list, LB_GETTOPINDEX, 0, 0));
    m_items = items;
    SendMessageW(m_list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(m_list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < items.size(); ++i)
    {
        std::wstring text = std::to_wstring(i + 1) + L". " + std::filesystem::path(items[i]).filename().wstring();
        if (static_cast<int>(i) == currentIndex) text = L"▶ " + text;
        SendMessageW(m_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    bool restoredSelection = false;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (std::find(selectedFiles.begin(), selectedFiles.end(), items[i]) != selectedFiles.end())
        {
            SendMessageW(m_list, LB_SETSEL, TRUE, static_cast<LPARAM>(i));
            restoredSelection = true;
        }
    }
    if (!restoredSelection && currentIndex >= 0 && currentIndex < static_cast<int>(items.size()))
        SendMessageW(m_list, LB_SETSEL, TRUE, currentIndex);
    auto oldCaret = std::find(items.begin(), items.end(), caretFile);
    if (oldCaret != items.end()) caret = static_cast<int>(oldCaret - items.begin());
    else caret = currentIndex >= 0 ? currentIndex : 0;
    if (!items.empty())
    {
        SendMessageW(m_list, LB_SETCARETINDEX, caret, FALSE);
        SendMessageW(m_list, LB_SETTOPINDEX, std::clamp(top, 0, static_cast<int>(items.size()) - 1), 0);
    }
    SendMessageW(m_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_list, nullptr, TRUE);
}

std::vector<std::wstring> PlaylistView::PromptOpenFiles()
{
    return FileDialogs::OpenVideoFiles(m_hwnd);
}

std::vector<int> PlaylistView::GetSelectedIndices() const
{
    int count = static_cast<int>(SendMessageW(m_list, LB_GETSELCOUNT, 0, 0));
    if (count <= 0) return {};
    std::vector<int> indices(count);
    int copied = static_cast<int>(SendMessageW(m_list, LB_GETSELITEMS, count, reinterpret_cast<LPARAM>(indices.data())));
    if (copied <= 0) return {};
    indices.resize(copied);
    return indices;
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
    case WM_CREATE:
    {
        auto instance = reinterpret_cast<CREATESTRUCTW*>(lParam)->hInstance;
        m_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS | LBS_EXTENDEDSEL,
            10, 10, 304, 320, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kList)), instance, nullptr);
        if (!m_list) return -1;
        SendMessageW(m_list, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        const wchar_t* labels[] = { L"선택 재생", L"파일 추가", L"선택 삭제", L"비우기" };
        for (int i = 0; i < 4; ++i)
        {
            HWND button = CreateWindowExW(0, L"BUTTON", labels[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                10 + i * 77, 340, 72, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlay + i)), instance, nullptr);
            if (!button) return -1;
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        }
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        const auto selection = GetSelectedIndices();
        if (id == kPlay || (id == kList && HIWORD(wParam) == LBN_DBLCLK))
        {
            if (m_onPlay) m_onPlay(selection);
        }
        else if (id == kAdd) { if (m_onAdd) m_onAdd(); }
        else if (id == kRemove && !selection.empty()) { if (m_onRemove) m_onRemove(selection); }
        else if (id == kClear) { if (m_onClear) m_onClear(); }
        return 0;
    }
    case WM_CLOSE:
        SetVisible(false);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        m_list = nullptr;
        m_items.clear();
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
