#pragma once

#include "Views/IControlBarView.h"
#include "Views/IMainView.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <shobjidl.h>
#include <wrl/client.h>

class MainView : public IMainView
{
public:
    explicit MainView(IControlBarView& controlBarView);
    ~MainView() override;

    bool Initialize(HINSTANCE hInstance);

    void Show(int cmdShow) override;
    HINSTANCE GetInstance() const override;
    HWND GetHwnd() const override;
    HWND GetVideoHwnd() const override;
    void SetVideoVisible(bool visible) override;
    void SetWindowTitle(const std::wstring& title) override;
    void SetStatusText(const std::wstring& statusText) override;
    void UpdateMenuState(bool repeatEnabled, bool subtitleVisible, bool topMost) override;
    void SetTopMost(bool topMost) override;
    void ToggleFullscreen() override;
    void ExitFullscreen() override;
    bool IsFullscreen() const override;
    void UpdateFullscreenAutoHide() override;

    std::vector<std::wstring> PromptOpenVideoFiles() override;
    std::wstring PromptOpenSubtitleFile() override;
    std::wstring PromptSaveScreenshotFile() override;
    void Render() override;
    void SetOnTimer(std::function<void(UINT_PTR)> handler) override;
    void SetOnPlaylistRequested(std::function<void()> handler) override;

    void SetOnOpenFileRequested(std::function<void()> handler) override;
    void SetOnOpenSubtitleRequested(std::function<void()> handler) override;
    void SetOnScreenshotRequested(std::function<void()> handler) override;
    void SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) override;
    void SetOnPlayPauseRequested(std::function<void()> handler) override;
    void SetOnStopRequested(std::function<void()> handler) override;
    void SetOnPrevRequested(std::function<void()> handler) override;
    void SetOnNextRequested(std::function<void()> handler) override;
    void SetOnSeekDirection(std::function<void(int)> handler) override;
    void SetOnVolumeStep(std::function<void(int)> handler) override;
    void SetOnRepeatToggled(std::function<void()> handler) override;
    void SetOnABLoopAction(std::function<void(int)> handler) override;
    void SetOnSubtitleToggle(std::function<void()> handler) override;
    void SetOnSubtitleDelayStep(std::function<void(double)> handler) override;
    void SetOnSubtitleScaleStep(std::function<void(double)> handler) override;
    void SetOnSubtitlePosStep(std::function<void(int)> handler) override;
    void SetOnTopMostToggled(std::function<void()> handler) override;
    void SetOnSettingsRequested(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    ATOM RegisterWindowClass(HINSTANCE hInstance) const;
    void RegisterHelperClasses(HINSTANCE hInstance) const;
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    void UpdateLayout();
    void RenderMainPaint(HDC hdc);

    void OnResize(UINT width, UINT height);
    void OnCommand(int commandId);
    void OnDropFiles(HDROP hDrop);
    void OnKeyDown(WPARAM key);

    IControlBarView& m_controlBarView;
    std::function<void(UINT_PTR)> m_onTimer;
    std::function<void()> m_onPlaylistRequested;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_videoHwnd = nullptr;
    HMENU m_hMenu = nullptr;


    bool m_isVideoVisible = false;
    bool m_isFullscreen = false;
    bool m_topMost = false;
    bool m_menuRepeat = false;
    bool m_menuSubVisible = true;
    bool m_menuTopMost = false;
    bool m_hasMenuState = false;

    WINDOWPLACEMENT m_savedPlacement{ sizeof(WINDOWPLACEMENT) };
    LONG_PTR m_savedStyle = 0;
    LONG_PTR m_savedExStyle = 0;
    ULONGLONG m_lastMouseActiveTick = 0;

    std::wstring m_windowClass;
    std::wstring m_defaultTitle;
    std::wstring m_currentTitle;
    std::wstring m_statusText;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<ITaskbarList2> m_taskbarList;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;

    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainTextBrush;

    std::function<void()> m_onOpenFileRequested;
    std::function<void()> m_onOpenSubtitleRequested;
    std::function<void()> m_onScreenshotRequested;
    std::function<void(const std::vector<std::wstring>&)> m_onFilesDropped;
    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onPrevRequested;
    std::function<void()> m_onNextRequested;
    std::function<void(int)> m_onSeekDirection;
    std::function<void(int)> m_onVolumeStep;
    std::function<void()> m_onRepeatToggled;
    std::function<void(int)> m_onABLoopAction;
    std::function<void()> m_onSubtitleToggle;
    std::function<void(double)> m_onSubtitleDelayStep;
    std::function<void(double)> m_onSubtitleScaleStep;
    std::function<void(int)> m_onSubtitlePosStep;
    std::function<void()> m_onTopMostToggled;
    std::function<void()> m_onSettingsRequested;
};
