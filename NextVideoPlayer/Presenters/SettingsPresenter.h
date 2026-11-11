#pragma once

#include "Models/ConfigService.h"
#include "Views/ISettingsView.h"

class SettingsPresenter
{
  public:
    SettingsPresenter(ISettingsView &view, ConfigService &configService);
    ~SettingsPresenter();

    bool ShowSettingsDialog(HINSTANCE hInstance, HWND parentHwnd, AppConfig &inOutConfig);
    void SetApiStatusProvider(std::function<std::wstring()> provider)
    {
        m_view.SetApiStatusProvider(std::move(provider));
    }
    void SetOnApiPageRequested(std::function<void()> handler)
    {
        m_openApiPage = std::move(handler);
    }

  private:
    void OnSaveClicked();
    void OnCancelClicked();
    bool SaveWorkingSettings(bool apiOnly = false);

    ISettingsView &m_view;
    ConfigService &m_configService;
    AppConfig m_workingConfig{};
    AppConfig m_initialConfig{};
    AppConfig *m_activeConfig = nullptr;
    std::function<void()> m_openApiPage;
};
