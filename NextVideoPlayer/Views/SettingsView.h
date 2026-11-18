#pragma once

#include "Views/ISettingsView.h"
#include "Models/AppInfo.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <functional>
#include <string>
#include <vector>

class SettingsView : public ISettingsView
{
public:
    explicit SettingsView(const AppInfo& appInfo, HFONT dialogFont = nullptr);
    ~SettingsView() override;

    bool ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig) override;
    AppConfig ReadFormValues(const AppConfig& baseConfig) const override;
    bool TryReadFormValues(const AppConfig& baseConfig, AppConfig& result) override;
    void SetApiStatusProvider(std::function<std::wstring()> provider) override
    {
        m_apiStatusProvider = std::move(provider);
    }
    void SetOnApiPageRequested(std::function<void()> handler) override
    {
        m_onApiPageRequested = std::move(handler);
    }
    void CloseModal(bool accepted) override;

    void SetOnSaveClicked(std::function<void()> handler) override;
    void SetOnCancelClicked(std::function<void()> handler) override;

    void SetPreviewStateForTest(int activeTab, bool showingApiGuide = false, bool showingLicenses = false);
    void DrawToTarget(ID2D1RenderTarget* rt, D2D1_SIZE_F size, bool isPreview = false);

    enum class ControlKind
    {
        Button,
        Toggle,
        StepperMinus,
        StepperPlus,
        Display,
        PrevSkip,
        NextSkip,
        UpArrow,
        DownArrow
    };

    struct ControlItem
    {
        int id = 0;
        D2D1_RECT_F rect{};
        std::wstring text;
        ControlKind kind = ControlKind::Button;
        bool state = false;
        bool disabled = false;
        bool accent = false;
    };

    struct BentoCard
    {
        D2D1_RECT_F rect{};
        std::wstring title;
        std::wstring subtitle;
    };

    struct CardLabel
    {
        D2D1_RECT_F rect{};
        std::wstring text;
        bool muted = false;
        bool bold = false;
        DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING;
    };

    struct FrameBox
    {
        D2D1_RECT_F rect{};
        bool focused = false;
    };

    struct ScrollTextBox
    {
        bool active = false;
        D2D1_RECT_F containerRect{};
        D2D1_RECT_F textRect{};
        D2D1_RECT_F trackRect{};
        D2D1_RECT_F thumbRect{};
        std::wstring text;
        float scrollOffset = 0.0f;
        float contentHeight = 0.0f;
        float maxScroll = 0.0f;
    };

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateDialogWindow();
    bool CreateApiEditors();
    void DestroyOwnedFontsAndBrushes();
    void RefreshEditFontForDpi(UINT dpi);
    void LayoutApiEditors();
    void CenterToParent(HWND parentHwnd, UINT dpi) const;
    void UpdateTabVisibility();
    void ToggleApiGuide();
    void OpenApiReadme();
    void ToggleLicenses();
    void OpenInfoLink(const std::wstring& target);
    void CopyTextWithFeedback(const std::wstring& text, const std::wstring& statusMessage);
    void CopyApiUrl();
    void DispatchParentCommand(int commandId, bool closeAfter = false);
    void ExecuteControl(int controlId);

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();

    void BuildActiveTabLayout(
        std::vector<BentoCard>& outCards,
        std::vector<CardLabel>& outLabels,
        std::vector<ControlItem>& outControls,
        std::vector<FrameBox>& outInputFrames,
        ScrollTextBox& outScrollBox);

    void PrepareScrollBoxMetrics(ScrollTextBox& box) const;
    void DrawScrollBox(ID2D1RenderTarget* rt, const ScrollTextBox& box) const;
    int HitTestControl(float x, float y) const;
    void Draw();

    void DrawGearIcon(ID2D1RenderTarget* rt, float cx, float cy, float radius) const;
    void DrawCloseIcon(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, ID2D1Brush* brush) const;
    void DrawStepperGlyph(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool isPlus, ID2D1Brush* brush) const;
    void DrawSkipGlyph(ID2D1RenderTarget* rt, float cx, float cy, bool isNext, ID2D1Brush* brush) const;
    void DrawToggleSwitch(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, bool state, bool hovered) const;

    HINSTANCE m_hInstance = nullptr;
    HFONT m_dialogFont = nullptr;
    HFONT m_ownedEditFont = nullptr;
    UINT m_currentDpi = 96;
    HWND m_hWnd = nullptr;
    HWND m_parentHwnd = nullptr;
    HWND m_ipEdit = nullptr;
    HWND m_portEdit = nullptr;
    HBRUSH m_editBackground = nullptr;
    bool m_ipFocused = false;
    bool m_portFocused = false;

    AppConfig m_currentConfig{};
    AppInfo m_appInfo{};
    int m_activeTab = 1; // Default to Playback tab matching view_settings mockup
    bool m_showingApiGuide = false;
    bool m_showingLicenses = false;
    bool m_running = false;
    bool m_accepted = false;
    int m_pendingPostCommand = 0;

    float m_apiSummaryScroll = 0.0f;
    float m_apiGuideScroll = 0.0f;
    float m_infoScroll = 0.0f;
    bool m_isDraggingScroll = false;
    float m_dragStartY = 0.0f;
    float m_dragStartScroll = 0.0f;

    std::wstring m_footerFeedback;
    ULONGLONG m_feedbackExpireTick = 0;

    int m_hoveredId = 0;
    int m_pressedId = 0;
    bool m_trackingMouse = false;

    std::function<void()> m_onSaveClicked;
    std::function<void()> m_onCancelClicked;
    std::function<std::wstring()> m_apiStatusProvider;
    std::function<void()> m_onApiPageRequested;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;

    struct Brushes
    {
        ID2D1RenderTarget* owner = nullptr;
        float gradHeight = 0.0f;

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> bg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> windowBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> divider;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tabTrackBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> tabTrackBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> activeTabBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> activeTabBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hoverTabBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> cardBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> cardBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> insetBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> insetBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> subText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> mutedText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> codeText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> darkText;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> cyan;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> cyanGlow;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> toggleOffBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> toggleOffBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> toggleOffKnob;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> whiteKnob;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> btnBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> btnHover;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> btnPressed;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> btnBorder;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accentBtnBg;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accentBtnHover;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> closeHover;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> scrollTrack;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> scrollThumb;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> saveGrad;

        void Reset();
        HRESULT Ensure(ID2D1RenderTarget* rt);
    };

    Brushes m_brushes{};
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_subTitleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tabFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tabBoldFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_cardHeaderFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_cardSubTitleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_labelFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_labelBoldFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnBoldFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_captionFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_scrollTextFormat;
};
