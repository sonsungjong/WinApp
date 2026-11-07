#pragma once

#include "Views/IControlBarView.h"
#include "Views/IVideoPlayerView.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wrl/client.h>

class VideoPlayerView : public IVideoPlayerView
{
public:
    explicit VideoPlayerView(IControlBarView& controlBarView);
    ~VideoPlayerView() override;

    bool Initialize(HINSTANCE hInstance, HWND parentHwnd);

    HINSTANCE GetInstance() const override;
    HWND GetHwnd() const override;
    HWND GetVideoHwnd() const override;
    void SetBounds(int x, int y, int width, int height) override;
    void SetVideoVisible(bool visible) override;
    void SetStatusText(const std::wstring& statusText) override;
    void SetFullscreenActive(bool active) override;
    void SetWindowActive(bool active) override;
    bool IsFullscreenActive() const override;
    void UpdateFullscreenAutoHide() override;

    std::vector<std::wstring> PromptOpenVideoFiles() override;
    std::wstring PromptOpenSubtitleFile() override;
    std::wstring PromptSaveScreenshotFile() override;
    void Render() override;

    void HandleCommand(int commandId) override;
    void HandleKeyDown(WPARAM key) override;
    void HandleMouseWheel(int delta) override;
    void HandleDropFiles(HDROP hDrop) override;

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
    void SetOnFullscreenToggleRequested(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void RegisterWindowClass(HINSTANCE hInstance) const;
    void RegisterHelperClasses(HINSTANCE hInstance) const;
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    void UpdateLayout();
    void RenderMainPaint(HDC hdc);
    void OnResize();

    HWND GetOwnerHwnd() const;
    bool IsVideoWindow(HWND hwnd) const;
    bool IsCursorOverVideo(POINT* position = nullptr) const;
    bool IsCursorAutoHideEnabled() const;
    void RestartCursorHideTimer();
    void ResetCursorAutoHide();
    void OnCursorActivity();
    void OnCursorHideTimer();

    IControlBarView& m_controlBarView;
    HINSTANCE m_hInstance = nullptr;
    HWND m_parentHwnd = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_videoHwnd = nullptr;

    bool m_isVideoVisible = false;
    bool m_isFullscreen = false;
    ULONGLONG m_lastMouseActiveTick = 0;
    bool m_isWindowActive = false;
    bool m_isCursorHidden = false;
    bool m_isCursorTimerArmed = false;
    bool m_isTrackingCursor = false;
    bool m_hasCursorPosition = false;
    POINT m_lastCursorPosition{};
    ULONGLONG m_cursorHideDue = 0;

    std::wstring m_windowClass;
    std::wstring m_statusText;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainTextBrush;

    std::function<void()> m_onPlaylistRequested;
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
    std::function<void()> m_onFullscreenToggleRequested;
};
