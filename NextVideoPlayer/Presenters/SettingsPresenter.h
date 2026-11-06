#pragma once

#include "Models/ConfigService.h"
#include "Views/ISettingsView.h"

class SettingsPresenter
{
public:
    SettingsPresenter(ISettingsView& view, ConfigService& configService);
    ~SettingsPresenter();

    bool ShowSettingsDialog(HINSTANCE hInstance, HWND parentHwnd, AppConfig& inOutConfig);

private:
    void OnSaveClicked();
    void OnCancelClicked();

    ISettingsView& m_view;
    ConfigService& m_configService;
    AppConfig m_workingConfig{};
};
