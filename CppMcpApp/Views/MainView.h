#pragma once

#include "framework.h"
#include "Views/IMainView.h"

// Application shell: top-level window, common menus and content layout only.
class MainView final : public IMainView
{
public:
    MainView() = default;
    ~MainView() override;
    MainView(const MainView&) = delete;
    MainView& operator=(const MainView&) = delete;

    bool Initialize(HINSTANCE instance);
    void Show(int cmdShow);
    HWND GetHwnd() const;
    void SetContentHwnd(HWND contentHwnd);

    void SetWindowTitle(const std::wstring& title) override;
    void ShowAbout() override;
    void Close() override;
    void SetOnCommand(std::function<void(int)> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK AboutDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void LayoutContent();

    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_contentHwnd = nullptr;
    std::function<void(int)> m_onCommand;
};
