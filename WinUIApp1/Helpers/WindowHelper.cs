using System;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using WinRT.Interop;
using Windows.Graphics;

namespace WinUIApp1.Helpers;

/// <summary>
/// 윈도우 조작 헬퍼 (풀스크린, TOPMOST 등)
/// </summary>
public static class WindowHelper
{
    /// <summary>
    /// 창을 풀스크린으로 전환
    /// </summary>
    public static void SetFullScreen(Window window)
    {
        var appWindow = GetAppWindow(window);
        if (appWindow != null)
        {
            appWindow.SetPresenter(AppWindowPresenterKind.FullScreen);
        }
    }

    /// <summary>
    /// 창을 일반 모드로 복원
    /// </summary>
    public static void RestoreWindow(Window window)
    {
        var appWindow = GetAppWindow(window);
        if (appWindow != null)
        {
            appWindow.SetPresenter(AppWindowPresenterKind.Default);
        }
    }

    /// <summary>
    /// 창을 항상 위에 표시 (TOPMOST)
    /// </summary>
    public static void SetTopmost(Window window, bool topmost)
    {
        var hwnd = WindowNative.GetWindowHandle(window);
        var hwndInsertAfter = topmost
            ? new IntPtr(-1) // HWND_TOPMOST
            : new IntPtr(-2); // HWND_NOTOPMOST

        Windows.Win32.PInvoke.SetWindowPos(
            new Windows.Win32.Foundation.HWND(hwnd),
            new Windows.Win32.Foundation.HWND(hwndInsertAfter),
            0, 0, 0, 0,
            Windows.Win32.UI.WindowsAndMessaging.SET_WINDOW_POS_FLAGS.SWP_NOMOVE |
            Windows.Win32.UI.WindowsAndMessaging.SET_WINDOW_POS_FLAGS.SWP_NOSIZE |
            Windows.Win32.UI.WindowsAndMessaging.SET_WINDOW_POS_FLAGS.SWP_NOACTIVATE);
    }

    /// <summary>
    /// 타이틀바 숨기기 (전체 화면 느낌)
    /// </summary>
    public static void HideTitleBar(Window window)
    {
        var appWindow = GetAppWindow(window);
        if (appWindow != null)
        {
            // Overlapped 프레젠터로 타이틀바 숨기기
            var presenter = appWindow.Presenter as OverlappedPresenter;
            if (presenter != null)
            {
                presenter.IsMaximizable = false;
                presenter.IsMinimizable = false;
                presenter.IsResizable = false;
                presenter.SetBorderAndTitleBar(false, false);
            }
        }
    }

    /// <summary>
    /// 창을 최대화하고 타이틀바 숨기기
    /// </summary>
    public static void MaximizeWithoutTitleBar(Window window)
    {
        var appWindow = GetAppWindow(window);
        if (appWindow != null)
        {
            // OverlappedPresenter 사용하여 최대화
            var presenter = OverlappedPresenter.CreateForContextMenu();
            appWindow.SetPresenter(presenter);
            presenter.Maximize();
        }
    }

    /// <summary>
    /// AppWindow 가져오기
    /// </summary>
    private static AppWindow? GetAppWindow(Window window)
    {
        var hwnd = WindowNative.GetWindowHandle(window);
        var windowId = Win32Interop.GetWindowIdFromWindow(hwnd);
        return AppWindow.GetFromWindowId(windowId);
    }

    /// <summary>
    /// 창 핸들 가져오기
    /// </summary>
    public static IntPtr GetHwnd(Window window)
    {
        return WindowNative.GetWindowHandle(window);
    }
}
