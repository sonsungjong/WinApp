#pragma once

#include "framework.h"
#include "Views/ISettingView.h"

// Owns the modal settings dialog. MainView only supplies its owner window.
class SettingView final : public ISettingView
{
public:
    SettingView(HINSTANCE instance, HWND owner);
    ~SettingView() override;
    SettingView(const SettingView&) = delete;
    SettingView& operator=(const SettingView&) = delete;

    void SetTitle(const std::wstring& title) override;
    void ShowModal() override;
    void Close() override;
    void SetOnCloseRequested(std::function<void()> handler) override;

private:
    static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(UINT message, WPARAM wParam);
    void RequestClose();

    HINSTANCE m_instance = nullptr;
    HWND m_owner = nullptr;
    HWND m_dialog = nullptr;
    std::wstring m_title;
    std::function<void()> m_onCloseRequested;
};
