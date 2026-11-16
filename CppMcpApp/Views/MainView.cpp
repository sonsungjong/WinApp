#include "Views/MainView.h"
#include "Resource.h"
#include <utility>

MainView::~MainView()
{
    m_onCommand = {};
    Close();
}

bool MainView::Initialize(HINSTANCE instance)
{
    if (m_hwnd)
        return false;

    m_instance = instance;
    wchar_t windowClass[128]{};
    if (!LoadStringW(instance, IDC_CPPMCPAPP, windowClass, ARRAYSIZE(windowClass)))
        return false;

    WNDCLASSEXW window{};
    window.cbSize = sizeof(window);
    window.style = CS_HREDRAW | CS_VREDRAW;
    window.lpfnWndProc = WindowProc;
    window.hInstance = instance;
    window.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_CPPMCPAPP));
    window.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SMALL));
    window.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window.lpszMenuName = MAKEINTRESOURCEW(IDC_CPPMCPAPP);
    window.lpszClassName = windowClass;
    if (!RegisterClassExW(&window) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    return CreateWindowExW(0, windowClass, L"", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, instance, this) != nullptr;
}

void MainView::Show(int cmdShow)
{
    ShowWindow(m_hwnd, cmdShow);
    UpdateWindow(m_hwnd);
}

HWND MainView::GetHwnd() const
{
    return m_hwnd;
}

void MainView::SetContentHwnd(HWND contentHwnd)
{
    m_contentHwnd = contentHwnd;
    LayoutContent();
}

void MainView::LayoutContent()
{
    if (!m_hwnd || !IsWindow(m_contentHwnd))
        return;

    RECT client{};
    GetClientRect(m_hwnd, &client);
    MoveWindow(m_contentHwnd, 0, 0, client.right - client.left, client.bottom - client.top, TRUE);
}

void MainView::SetWindowTitle(const std::wstring& title)
{
    SetWindowTextW(m_hwnd, title.c_str());
}

void MainView::ShowAbout()
{
    DialogBoxParamW(m_instance, MAKEINTRESOURCEW(IDD_ABOUTBOX), m_hwnd, AboutDialogProc, 0);
}

void MainView::Close()
{
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

void MainView::SetOnCommand(std::function<void(int)> handler)
{
    m_onCommand = std::move(handler);
}

LRESULT CALLBACK MainView::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* view = reinterpret_cast<MainView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        view = static_cast<MainView*>(create->lpCreateParams);
        view->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    return view ? view->HandleMessage(message, wParam, lParam)
                : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT MainView::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        LayoutContent();
        return 0;
    case WM_COMMAND:
        if (m_onCommand)
            m_onCommand(LOWORD(wParam));
        return 0;
    case WM_CLOSE:
        if (m_onCommand)
            m_onCommand(IDM_EXIT);
        else
            Close();
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        BeginPaint(m_hwnd, &paint);
        EndPaint(m_hwnd, &paint);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
    {
        const HWND hwnd = m_hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        m_contentHwnd = nullptr;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    default:
        return DefWindowProcW(m_hwnd, message, wParam, lParam);
    }
}

INT_PTR CALLBACK MainView::AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    if (message == WM_INITDIALOG)
        return TRUE;
    if (message == WM_COMMAND && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL))
    {
        EndDialog(dialog, LOWORD(wParam));
        return TRUE;
    }
    return FALSE;
}
