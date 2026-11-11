#pragma once

#include "Views/ISettingsView.h"
#include "Models/AppInfo.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wrl/client.h>
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
      m_apiStatus = std::move(provider);
  }
  void SetOnApiPageRequested(std::function<void()> handler) override
  {
      m_onApiPage = std::move(handler);
  }
    void CloseModal(bool accepted) override;

    void SetOnSaveClicked(std::function<void()> handler) override;
    void SetOnCancelClicked(std::function<void()> handler) override;

private:
    struct ControlItem
    {
        int id;
        D2D1_RECT_F rect;
        std::wstring label;
        bool isToggle = false;
        bool toggleState = false;
        bool isDisplayOnly = false;
    };

    struct BentoCard
    {
        D2D1_RECT_F rect;
        std::wstring title;
    };

    struct CardLabel
    {
        D2D1_RECT_F rect;
        std::wstring text;
    };

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    ATOM RegisterWindowClass(HINSTANCE hInstance) const;

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    void Draw();
    void DrawGearIcon(float cx, float cy, float radius);
    void DrawToggleSwitch(const D2D1_RECT_F& rect, bool state);

    D2D1_RECT_F GetCloseButtonRect() const;
    D2D1_RECT_F GetTabRect(int tabIndex) const;
    D2D1_RECT_F GetCancelButtonRect() const;
    D2D1_RECT_F GetSaveButtonRect() const;
    void BuildActiveTabLayout(
        std::vector<BentoCard>& outCards,
        std::vector<CardLabel>& outLabels,
        std::vector<ControlItem>& outControls) const;

    int HitTest(float x, float y) const;
    void ExecuteControl(int controlId);
    void DispatchParentCommand(int commandId, bool closeAfter = false);
    bool CreateApiEditors();
    void UpdateTabVisibility();
    void CopyApiAddress();
    bool CreateInfoEditor();
    void ToggleLicenses();
    void OpenInfoLink(const std::wstring& target);

    AppConfig m_initialConfig{};
    AppConfig m_currentConfig{};
    bool m_accepted = false;
    bool m_done = false;
    int m_pendingPostCommand = 0;

    HWND m_hWnd = nullptr;
    HWND m_parentHwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HFONT m_dialogFont = nullptr;
    HBRUSH m_editBackground = nullptr;
    HWND m_ipEdit = nullptr, m_portEdit = nullptr;
    HWND m_infoEdit = nullptr;
    const AppInfo& m_appInfo;
    bool m_showingLicenses = false;
    std::function<std::wstring()> m_apiStatus;
    std::function<void()> m_onApiPage;
    ULONGLONG m_apiCopiedUntil = 0;

    int m_activeTab = 1; // Default to Playback tab (matches view_settings.png!)
    int m_hoveredId = -1;
    int m_pressedId = -1;
    bool m_trackingMouse = false;

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteFontCollection1> m_customFontCollection;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleBoldFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_titleSubFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tabFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_tabActiveFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_cardTitleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_rowLabelFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_valueFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_btnBoldFormat;

    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_bgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_footerBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_windowBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_tabActiveBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cardBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_separatorBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_subTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_darkTextBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cyanBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_cyanGlowBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_toggleOffBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_toggleOffBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_toggleOffThumbBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_toggleOnThumbBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnPressedBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_btnBorderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_closeHoverBrush;
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> m_saveBtnGradBrush;

    std::function<void()> m_onSaveClicked;
    std::function<void()> m_onCancelClicked;
};
