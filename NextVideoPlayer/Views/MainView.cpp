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

    RECT clientRect{};
    GetClientRect(m_hWnd, &clientRect);

    m_videoHwnd = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_CLIPSIBLINGS,
        0,
        0,
        clientRect.right - clientRect.left,
        clientRect.bottom - clientRect.top,
        m_hWnd,
        nullptr,
        hInstance,
        nullptr);

    if (!m_videoHwnd)
    {
        return false;
    }

    EnableWindow(m_videoHwnd, FALSE);
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
    m_isVideoVisible = visible;
    if (m_videoHwnd)
    {
        ShowWindow(m_videoHwnd, visible ? SW_SHOW : SW_HIDE);
    }

    if (!visible)
    {
        Render();
    }
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
    if (!m_isVideoVisible)
    {
        Render();
    }
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

void MainView::Render()
{
    if (!m_hWnd || m_isVideoVisible)
    {
        return;
    }

    if (FAILED(CreateDeviceResources()))
    {
        return;
    }

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0.05f, 0.05f, 0.05f));

    if (!m_statusText.empty() && m_textFormat && m_textBrush)
    {
        D2D1_SIZE_F size = m_renderTarget->GetSize();
        D2D1_RECT_F layoutRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
        m_renderTarget->DrawTextW(
            m_statusText.c_str(),
            static_cast<UINT32>(m_statusText.length()),
            m_textFormat.Get(),
            layoutRect,
            m_textBrush.Get());
    }

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
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
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NEXTVIDEOPLAYER);
    wcex.lpszClassName = m_windowClass.c_str();
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL));

    return RegisterClassExW(&wcex);
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

    hr = m_dwriteFactory->CreateTextFormat(
        L"Malgun Gothic",
        nullptr,
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

    return m_renderTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.88f, 0.88f, 0.88f),
        m_textBrush.GetAddressOf());
}

void MainView::DiscardDeviceResources()
{
    m_textBrush.Reset();
    m_renderTarget.Reset();
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

void MainView::OnResize(UINT width, UINT height)
{
    if (m_videoHwnd)
    {
        MoveWindow(m_videoHwnd, 0, 0, static_cast<int>(width), static_cast<int>(height), TRUE);
    }

    if (m_renderTarget)
    {
        m_renderTarget->Resize(D2D1::SizeU(width, height));
    }
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
