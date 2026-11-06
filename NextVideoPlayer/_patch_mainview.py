from pathlib import Path
p = Path(r'C:\Users\cbmplus\source\WinApp\NextVideoPlayer\Views\MainView.cpp')
text = p.read_text(encoding='utf-8')

helper = '''    void ShowExplorerTaskbar(bool show)
    {
        const int cmd = show ? SW_SHOW : SW_HIDE;
        if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr))
        {
            ShowWindow(tray, cmd);
        }

        HWND secondary = nullptr;
        while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr)
        {
            ShowWindow(secondary, cmd);
        }
    }

'''

old_ns_end = '''        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

}
'''
new_ns_end = '''        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

''' + helper + '''}
'''
if old_ns_end not in text:
    raise SystemExit('namespace end not found')
text = text.replace(old_ns_end, new_ns_end, 1)

old_dtor = '''MainView::~MainView()
{
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}
'''
new_dtor = '''MainView::~MainView()
{
    if (m_isFullscreen)
    {
        ShowExplorerTaskbar(true);
        m_isFullscreen = false;
    }
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}
'''
if old_dtor not in text:
    raise SystemExit('dtor not found')
text = text.replace(old_dtor, new_dtor, 1)

old_ex = '''            m_savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
'''
new_ex = '''            (m_savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)) | WS_EX_TOPMOST);
'''
if old_ex not in text:
    raise SystemExit('exstyle not found')
text = text.replace(old_ex, new_ex, 1)

old_mark = '''        if (m_taskbarList)
        {
            m_taskbarList->MarkFullscreenWindow(m_hWnd, TRUE);
        }

        UpdateLayout();
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hWnd);
'''
new_mark = '''        if (m_taskbarList)
        {
            m_taskbarList->MarkFullscreenWindow(m_hWnd, TRUE);
        }

        UpdateLayout();
        SetForegroundWindow(m_hWnd);
        SetFocus(m_hWnd);
        ShowExplorerTaskbar(false);
'''
if old_mark not in text:
    raise SystemExit('enter fullscreen mark not found')
text = text.replace(old_mark, new_mark, 1)

old_exit = '''    m_isFullscreen = false;

    if (m_taskbarList)
    {
        m_taskbarList->MarkFullscreenWindow(m_hWnd, FALSE);
    }
'''
new_exit = '''    m_isFullscreen = false;
    ShowExplorerTaskbar(true);

    if (m_taskbarList)
    {
        m_taskbarList->MarkFullscreenWindow(m_hWnd, FALSE);
    }
'''
if old_exit not in text:
    raise SystemExit('exit fullscreen not found')
text = text.replace(old_exit, new_exit, 1)

old_auto = '''    if (!m_hWnd || !m_isFullscreen)
    {
        return;
    }

    POINT cursorPt{};
'''
new_auto = '''    if (!m_hWnd || !m_isFullscreen)
    {
        return;
    }

    ShowExplorerTaskbar(false);

    POINT cursorPt{};
'''
if old_auto not in text:
    raise SystemExit('autohide not found')
text = text.replace(old_auto, new_auto, 1)

old_destroy = '''    case WM_DESTROY:
        DiscardDeviceResources();
'''
new_destroy = '''    case WM_DESTROY:
        if (m_isFullscreen)
        {
            ShowExplorerTaskbar(true);
            m_isFullscreen = false;
        }
        DiscardDeviceResources();
'''
if old_destroy not in text:
    raise SystemExit('wm_destroy not found')
text = text.replace(old_destroy, new_destroy, 1)

p.write_text(text, encoding='utf-8')
print('patched ok')
