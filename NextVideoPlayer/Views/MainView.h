#pragma once

#include "Views/IMainView.h"
#include <shobjidl.h>
#include <wrl/client.h>

class MainView : public IMainView
{
public:
    MainView();
    ~MainView() override;

    bool Initialize(HINSTANCE hInstance);

    void Show(int cmdShow) override;
    HINSTANCE GetInstance() const override;
    HWND GetHwnd() const override;
    void SetWindowTitle(const std::wstring& title) override;
    void UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost) override;
    void SetTopMost(bool topMost) override;
    void ToggleFullscreen() override;
    void ExitFullscreen() override;
    bool IsFullscreen() const override;
    void SetPlayerHwnd(HWND playerHwnd) override;

    void SetOnTimer(std::function<void(UINT_PTR)> handler) override;
    void SetOnCommand(std::function<void(int)> handler) override;
    void SetOnKeyDown(std::function<void(WPARAM)> handler) override;
    void SetOnMouseWheel(std::function<void(int)> handler) override;
    void SetOnFilesDropped(std::function<void(HDROP)> handler) override;
    void SetOnFullscreenChanged(std::function<void(bool)> handler) override;
    void SetOnActivationChanged(std::function<void(bool)> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    ATOM RegisterWindowClass(HINSTANCE hInstance) const;
    void LayoutPlayer();
    void OnResize();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_playerHwnd = nullptr;
    HMENU m_hMenu = nullptr;

    bool m_isFullscreen = false;
    bool m_topMost = false;
    bool m_menuRepeat = false;
    bool m_menuSubVisible = true;
    bool m_menuTopMost = false;
    bool m_hasMenuState = false;

    WINDOWPLACEMENT m_savedPlacement{ sizeof(WINDOWPLACEMENT) };
    LONG_PTR m_savedStyle = 0;
    LONG_PTR m_savedExStyle = 0;

    std::wstring m_windowClass;
    std::wstring m_defaultTitle;
    std::wstring m_currentTitle;

    Microsoft::WRL::ComPtr<ITaskbarList2> m_taskbarList;

    std::function<void(UINT_PTR)> m_onTimer;
    std::function<void(int)> m_onCommand;
    std::function<void(WPARAM)> m_onKeyDown;
    std::function<void(int)> m_onMouseWheel;
    std::function<void(HDROP)> m_onFilesDropped;
    std::function<void(bool)> m_onFullscreenChanged;
    std::function<void(bool)> m_onActivationChanged;
};
