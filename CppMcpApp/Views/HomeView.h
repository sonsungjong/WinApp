#pragma once

#include "framework.h"
#include "Views/IHomeView.h"

// The content screen hosted inside MainView. Add Home controls and rendering here.
class HomeView final : public IHomeView
{
public:
    HomeView() = default;
    ~HomeView() override;
    HomeView(const HomeView&) = delete;
    HomeView& operator=(const HomeView&) = delete;

    bool Initialize(HINSTANCE instance, HWND parent);
    HWND GetHwnd() const;
    void SetTitle(const std::wstring& title) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    std::wstring m_title;
};
