#include "Views/HomeView.h"

namespace
{
    constexpr wchar_t HomeWindowClass[] = L"CppMcpApp.HomeView";
}

HomeView::~HomeView()
{
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

bool HomeView::Initialize(HINSTANCE instance, HWND parent)
{
    if (m_hwnd || !IsWindow(parent))
        return false;

    WNDCLASSEXW window{};
    window.cbSize = sizeof(window);
    window.style = CS_HREDRAW | CS_VREDRAW;
    window.lpfnWndProc = WindowProc;
    window.hInstance = instance;
    window.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window.lpszClassName = HomeWindowClass;
    if (!RegisterClassExW(&window) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    return CreateWindowExW(0, HomeWindowClass, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0, parent, nullptr, instance, this) != nullptr;
}

HWND HomeView::GetHwnd() const
{
    return m_hwnd;
}

void HomeView::SetTitle(const std::wstring& title)
{
    m_title = title;
    SetWindowTextW(m_hwnd, title.c_str());
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK HomeView::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* view = reinterpret_cast<HomeView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        view = static_cast<HomeView*>(create->lpCreateParams);
        view->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    return view ? view->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT HomeView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(m_hwnd, &paint);
        RECT content{};
        GetClientRect(m_hwnd, &content);
        content.left += 16;
        content.top += 16;
        const HGDIOBJ previousFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        DrawTextW(dc, m_title.c_str(), static_cast<int>(m_title.size()), &content,
            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, previousFont);
        EndPaint(m_hwnd, &paint);
        return 0;
    }
    case WM_NCDESTROY:
    {
        const HWND hwnd = m_hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    default:
        // Only the Main window owns application shutdown/PostQuitMessage.
        return DefWindowProcW(m_hwnd, message, wParam, lParam);
    }
}
