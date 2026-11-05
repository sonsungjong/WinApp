#include "Views/MainView.h"
#include "Resource.h"

namespace
{
    constexpr int kMaxLoadString = 100;
    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 720;
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
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kDefaultWidth,
        kDefaultHeight,
        nullptr,
        nullptr,
        hInstance,
        this);

    return m_hWnd != nullptr;
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
    RefreshView();
}

void MainView::SetRepeatChecked(bool checked)
{
    if (m_hWnd)
    {
        HMENU hMenu = GetMenu(m_hWnd);
        if (hMenu)
        {
            CheckMenuItem(hMenu, IDM_REPEAT_TOGGLE, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
        }
    }
}

std::wstring MainView::PromptOpenVideoFile()
{
    WCHAR fileBuffer[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"동영상 파일 (*.mp4;*.mkv;*.avi;*.wmv;*.mov)\0*.mp4;*.mkv;*.avi;*.wmv;*.mov\0모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn))
    {
        return fileBuffer;
    }

    return {};
}

void MainView::RefreshView()
{
    if (m_hWnd)
    {
        InvalidateRect(m_hWnd, nullptr, TRUE);
    }
}

void MainView::SetOnOpenFileRequested(std::function<void()> handler)
{
    m_onOpenFileRequested = std::move(handler);
}

void MainView::SetOnFileDropped(std::function<void(const std::wstring&)> handler)
{
    m_onFileDropped = std::move(handler);
}

void MainView::SetOnPlayPauseRequested(std::function<void()> handler)
{
    m_onPlayPauseRequested = std::move(handler);
}

void MainView::SetOnStopRequested(std::function<void()> handler)
{
    m_onStopRequested = std::move(handler);
}

void MainView::SetOnRepeatToggled(std::function<void()> handler)
{
    m_onRepeatToggled = std::move(handler);
}

ATOM MainView::RegisterWindowClass(HINSTANCE hInstance) const
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainView::WindowProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_NEXTVIDEOPLAYER));
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NEXTVIDEOPLAYER);
    wcex.lpszClassName = m_windowClass.c_str();
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL));

    return RegisterClassExW(&wcex);
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
    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_SPACE && m_onPlayPauseRequested)
        {
            m_onPlayPauseRequested();
            return 0;
        }
        break;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(m_hWnd, message, wParam, lParam);
}

void MainView::OnPaint()
{
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(m_hWnd, &ps);

    RECT clientRect{};
    GetClientRect(m_hWnd, &clientRect);
    FillRect(hdc, &clientRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    if (!m_statusText.empty())
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(220, 220, 220));
        DrawTextW(
            hdc,
            m_statusText.c_str(),
            -1,
            &clientRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    EndPaint(m_hWnd, &ps);
}

void MainView::OnCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_FILE_OPEN:
        if (m_onOpenFileRequested)
        {
            m_onOpenFileRequested();
        }
        break;

    case IDM_PLAY_PAUSE:
        if (m_onPlayPauseRequested)
        {
            m_onPlayPauseRequested();
        }
        break;

    case IDM_STOP:
        if (m_onStopRequested)
        {
            m_onStopRequested();
        }
        break;

    case IDM_REPEAT_TOGGLE:
        if (m_onRepeatToggled)
        {
            m_onRepeatToggled();
        }
        break;

    case IDM_EXIT:
        DestroyWindow(m_hWnd);
        break;
    }
}

void MainView::OnDropFiles(HDROP hDrop)
{
    WCHAR filePath[MAX_PATH]{};
    if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH) > 0 && m_onFileDropped)
    {
        m_onFileDropped(filePath);
    }
    DragFinish(hDrop);
}

