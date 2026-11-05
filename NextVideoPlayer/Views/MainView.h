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
    HWND GetVideoHwnd() const override;
    void SetVideoVisible(bool visible) override;
    void SetWindowTitle(const std::wstring& title) override;
    void SetStatusText(const std::wstring& statusText) override;
    void SetRepeatChecked(bool checked) override;
    std::wstring PromptOpenVideoFile() override;
    void Render() override;

    void SetOnOpenFileRequested(std::function<void()> handler) override;
    void SetOnFileDropped(std::function<void(const std::wstring&)> handler) override;
    void SetOnPlayPauseRequested(std::function<void()> handler) override;
    void SetOnStopRequested(std::function<void()> handler) override;
    void SetOnRepeatToggled(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    ATOM RegisterWindowClass(HINSTANCE hInstance) const;

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    void OnResize(UINT width, UINT height);
    void OnCommand(int commandId);
    void OnDropFiles(HDROP hDrop);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_videoHwnd = nullptr;
    bool m_isVideoVisible = false;
    std::wstring m_windowClass;
    std::wstring m_defaultTitle;
    std::wstring m_statusText;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;

    std::function<void()> m_onOpenFileRequested;
    std::function<void(const std::wstring&)> m_onFileDropped;
    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onRepeatToggled;
};
