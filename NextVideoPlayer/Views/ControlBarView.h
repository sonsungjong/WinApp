#pragma once

#include "Views/IControlBarView.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wrl/client.h>

class ControlBarView : public IControlBarView
{
public:
    ControlBarView() = default;
    ~ControlBarView() override;

    bool Initialize(HINSTANCE hInstance, HWND parentHwnd, int x, int y, int width, int height) override;
    HWND GetHwnd() const override;
    void SetBounds(int x, int y, int width, int height, bool visible) override;
    void SetVisible(bool visible) override;
    bool IsVisible() const override;
    bool IsDragging() const override;
    void Render(const ControlBarState& state) override;
    void Invalidate() override;

    void SetOnPlayPauseRequested(std::function<void()> handler) override;
    void SetOnStopRequested(std::function<void()> handler) override;
    void SetOnPrevRequested(std::function<void()> handler) override;
    void SetOnNextRequested(std::function<void()> handler) override;
    void SetOnSeekAbsolute(std::function<void(double)> handler) override;
    void SetOnVolumeChanged(std::function<void(int)> handler) override;
    void SetOnMuteToggled(std::function<void()> handler) override;
    void SetOnSpeedStep(std::function<void(double)> handler) override;
    void SetOnSpeedReset(std::function<void()> handler) override;
    void SetOnRepeatToggled(std::function<void()> handler) override;
    void SetOnSubtitleToggle(std::function<void()> handler) override;
    void SetOnPlaylistRequested(std::function<void()> handler) override;
    void SetOnSettingsRequested(std::function<void()> handler) override;

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    ATOM RegisterWindowClass(HINSTANCE hInstance) const;
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    void Draw(HDC hdc);
    void DrawButton(const D2D1_RECT_F& rect, const std::wstring& label, IDWriteTextFormat* textFormat, bool active = false);
    static std::wstring FormatTime(double seconds);
    static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y);

    void OnLButtonDown(float x, float y);
    void OnLButtonUp(float x, float y);
    void CancelDrag();
    void OnMouseMove(float x, float y);
    void OnMouseLeave();
    void SeekFromMouseX(float x);
    void VolumeFromMouseX(float x);

    HINSTANCE m_hInstance = nullptr;
    HWND m_parentHwnd = nullptr;
    HWND m_hWnd = nullptr;
    bool m_isVisible = true;

    ControlBarState m_state{};
    bool m_isDraggingSeek = false;
    bool m_isDraggingVolume = false;
    bool m_isTrackingMouse = false;
    D2D1_POINT_2F m_hoverMousePt{ -1.0f, -1.0f };

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
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_uiTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_iconTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_timerTextFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_speedTextFormat;

    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> m_renderTarget;
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

    std::function<void()> m_onPlayPauseRequested;
    std::function<void()> m_onStopRequested;
    std::function<void()> m_onPrevRequested;
    std::function<void()> m_onNextRequested;
    std::function<void(double)> m_onSeekAbsolute;
    std::function<void(int)> m_onVolumeChanged;
    std::function<void()> m_onMuteToggled;
    std::function<void(double)> m_onSpeedStep;
    std::function<void()> m_onSpeedReset;
    std::function<void()> m_onRepeatToggled;
    std::function<void()> m_onSubtitleToggle;
    std::function<void()> m_onPlaylistRequested;
    std::function<void()> m_onSettingsRequested;
};
