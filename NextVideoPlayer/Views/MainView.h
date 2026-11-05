#pragma once

#include "Views/IMainView.h"

class MainView : public IMainView
{
public:
    MainView() = default;
    ~MainView() override = default;

    bool Initialize(HINSTANCE hInstance);

    void Show(int cmdShow) override;
    HWND GetHwnd() const override;
    void SetWindowTitle(const std::wstring& title) override;
    void SetStatusText(const std::wstring& statusText) override;
    void SetRepeatChecked(bool checked) override;
    std::wstring PromptOpenVideoFile() override;
    void RefreshView() override;

    void SetOnOpenFileRequested(std::function<void()> handler) override;
    void SetOnFileDropped(std::function<void(const std::wstring&)> handler) override;
    void SetOnPlayPauseRequested(std::function<void()> handler) override;
    void SetOnStopRequested(std::function<void()> handler) override;
    void SetOnRepeatToggled(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    ATOM RegisterWindowClass(HINSTANCE hInstance) const;
    void OnPaint();
    void OnCommand(int commandId);
    void OnDropFiles(HDROP hDrop);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    std::wstring m_windowClass;
    std::wstring m_defaultTitle;
    std::wstring m_statusText;

    std::function<void()> m_onOpenFileRequested;
    std::function<void(const std::wstring&)> m_onFileDropped;
    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onRepeatToggled;
};

