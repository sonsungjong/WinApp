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
    void UpdateControlBar(const ControlBarState& state) override;
    void UpdatePlaylistView(const std::vector<std::wstring>& items, int currentIndex) override;
    void SetTopMost(bool topMost) override;
    void ToggleFullscreen() override;
    void ExitFullscreen() override;
    bool IsFullscreen() const override;
    void UpdateFullscreenAutoHide() override;

    std::wstring PromptOpenVideoFile() override;
    std::wstring PromptOpenSubtitleFile() override;
    std::wstring PromptSaveScreenshotFile() override;
    bool ShowSettingsModal(AppConfig& inOutConfig) override;
    void TogglePlaylistWindow() override;
    void Render() override;

    void SetOnOpenFileRequested(std::function<void()> handler) override;
    void SetOnOpenSubtitleRequested(std::function<void()> handler) override;
    void SetOnScreenshotRequested(std::function<void()> handler) override;
    void SetOnFilesDropped(std::function<void(const std::vector<std::wstring>&)> handler) override;
    void SetOnPlayPauseRequested(std::function<void()> handler) override;
    void SetOnStopRequested(std::function<void()> handler) override;
    void SetOnPrevRequested(std::function<void()> handler) override;
    void SetOnNextRequested(std::function<void()> handler) override;
    void SetOnSeekAbsolute(std::function<void(double)> handler) override;
    void SetOnSeekDirection(std::function<void(int)> handler) override;
    void SetOnVolumeChanged(std::function<void(int)> handler) override;
    void SetOnVolumeStep(std::function<void(int)> handler) override;
    void SetOnMuteToggled(std::function<void()> handler) override;
    void SetOnSpeedStep(std::function<void(double)> handler) override;
    void SetOnSpeedReset(std::function<void()> handler) override;
    void SetOnRepeatToggled(std::function<void()> handler) override;
    void SetOnABLoopAction(std::function<void(int)> handler) override;
    void SetOnSubtitleToggle(std::function<void()> handler) override;
    void SetOnSubtitleDelayStep(std::function<void(double)> handler) override;
    void SetOnSubtitleScaleStep(std::function<void(double)> handler) override;
    void SetOnSubtitlePosStep(std::function<void(int)> handler) override;
    void SetOnTopMostToggled(std::function<void()> handler) override;
    void SetOnSettingsRequested(std::function<void()> handler) override;
    void SetOnPlaylistPlayIndex(std::function<void(int)> handler) override;
    void SetOnPlaylistAddFiles(std::function<void()> handler) override;
    void SetOnPlaylistRemoveIndex(std::function<void(int)> handler) override;
    void SetOnPlaylistClear(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ControlBarWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PlaylistWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleControlBarMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandlePlaylistMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    ATOM RegisterWindowClass(HINSTANCE hInstance) const;
    void RegisterHelperClasses(HINSTANCE hInstance) const;

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    HRESULT CreateControlBarResources();
    void DiscardControlBarResources();

    void UpdateLayout();
    void SetControlBarVisible(bool visible);
    void DrawControlBar();
    void DrawButton(const D2D1_RECT_F& rect, const std::wstring& label, IDWriteTextFormat* textFormat, bool active = false);
    static std::wstring FormatTime(double seconds);
    static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y);

    void OnResize(UINT width, UINT height);
    void OnCommand(int commandId);
    void OnDropFiles(HDROP hDrop);
    void OnKeyDown(WPARAM key);
    void OnControlBarLButtonDown(float x, float y);
    void OnControlBarLButtonUp(float x, float y);
    void OnControlBarMouseMove(float x, float y);
    void OnControlBarMouseLeave();
    void SeekFromMouseX(float x);
    void VolumeFromMouseX(float x);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_videoHwnd = nullptr;
    HWND m_controlBarHwnd = nullptr;
    HMENU m_hMenu = nullptr;

    HWND m_playlistHwnd = nullptr;
    HWND m_playlistListBox = nullptr;
    std::vector<std::wstring> m_cachedPlaylist;
    int m_cachedPlaylistIndex = -1;

    bool m_isVideoVisible = false;
    bool m_isFullscreen = false;
    bool m_isControlBarVisible = true;
    WINDOWPLACEMENT m_savedPlacement{ sizeof(WINDOWPLACEMENT) };
    LONG_PTR m_savedStyle = 0;
    LONG_PTR m_savedExStyle = 0;

    POINT m_lastMousePt{ -1, -1 };
    ULONGLONG m_lastMouseActiveTick = 0;
    bool m_isDraggingSeek = false;
    bool m_isDraggingVolume = false;
    bool m_isTrackingBarMouse = false;
    D2D1_POINT_2F m_hoverMousePt{ -1.0f, -1.0f };

    std::wstring m_windowClass;
    std::wstring m_defaultTitle;
    std::wstring m_statusText;
    ControlBarState m_barState;

    D2D1_RECT_F m_seekBarRect{};
    D2D1_RECT_F m_volSliderRect{};
    D2D1_RECT_F m_btnPlayPause{};
    D2D1_RECT_F m_btnStop{};
    D2D1_RECT_F m_btnPrev{};
    D2D1_RECT_F m_btnNext{};
    D2D1_RECT_F m_btnMute{};
    D2D1_RECT_F m_btnSpeedMinus{};
    D2D1_RECT_F m_btnSpeedReset{};
    D2D1_RECT_F m_btnSpeedPlus{};
    D2D1_RECT_F m_btnRepeat{};
    D2D1_RECT_F m_btnSub{};
    D2D1_RECT_F m_btnPlaylist{};
    D2D1_RECT_F m_btnSettings{};

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<ITaskbarList2> m_taskbarList;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_uiTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_iconTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timerTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_speedTextFormat;

    // Main window direct2d
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_mainTextBrush;

    // Control bar direct2d
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_barRenderTarget;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_barGradientBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_barTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_barSubTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_barBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_accentBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_accentHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_trackBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_trackHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_abMarkerBrush;

    std::function<void()> m_onOpenFileRequested;
    std::function<void()> m_onOpenSubtitleRequested;
    std::function<void()> m_onScreenshotRequested;
    std::function<void(const std::vector<std::wstring>&)> m_onFilesDropped;
    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onPrevRequested;
    std::function<void()> m_onNextRequested;
    std::function<void(double)> m_onSeekAbsolute;
    std::function<void(int)> m_onSeekDirection;
    std::function<void(int)> m_onVolumeChanged;
    std::function<void(int)> m_onVolumeStep;
    std::function<void()> m_onMuteToggled;
    std::function<void(double)> m_onSpeedStep;
    std::function<void()> m_onSpeedReset;
    std::function<void()> m_onRepeatToggled;
    std::function<void(int)> m_onABLoopAction;
    std::function<void()> m_onSubtitleToggle;
    std::function<void(double)> m_onSubtitleDelayStep;
    std::function<void(double)> m_onSubtitleScaleStep;
    std::function<void(int)> m_onSubtitlePosStep;
    std::function<void()> m_onTopMostToggled;
    std::function<void()> m_onSettingsRequested;
    std::function<void(int)> m_onPlaylistPlayIndex;
    std::function<void()> m_onPlaylistAddFiles;
    std::function<void(int)> m_onPlaylistRemoveIndex;
    std::function<void()> m_onPlaylistClear;
};
