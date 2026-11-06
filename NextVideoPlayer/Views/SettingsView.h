#pragma once

#include "Views/ISettingsView.h"

class SettingsView : public ISettingsView
{
public:
    explicit SettingsView(HFONT dialogFont) : m_dialogFont(dialogFont) {}
    ~SettingsView() override;

    bool ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig) override;
    AppConfig ReadFormValues(const AppConfig& baseConfig) const override;
    void CloseModal(bool accepted) override;

    void SetOnSaveClicked(std::function<void()> handler) override;
    void SetOnCancelClicked(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    ATOM RegisterWindowClass(HINSTANCE hInstance) const;

    AppConfig m_initialConfig{};
    bool m_accepted = false;
    bool m_done = false;

    HWND m_hWnd = nullptr;
    HFONT m_dialogFont = nullptr;
    HWND m_hSeekEdit = nullptr;
    HWND m_hRepeatChk = nullptr;
    HWND m_hTopMostChk = nullptr;
    HWND m_hSubChk = nullptr;

    std::function<void()> m_onSaveClicked;
    std::function<void()> m_onCancelClicked;
};
