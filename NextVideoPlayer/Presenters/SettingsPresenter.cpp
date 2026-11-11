#include "Presenters/SettingsPresenter.h"

SettingsPresenter::SettingsPresenter(ISettingsView &view, ConfigService &configService)
    : m_view(view), m_configService(configService)
{
    m_view.SetOnSaveClicked([this]() { OnSaveClicked(); });
    m_view.SetOnCancelClicked([this]() { OnCancelClicked(); });
    m_view.SetOnApiPageRequested([this]() {
        if (SaveWorkingSettings(true) && m_openApiPage)
            m_openApiPage();
    });
}

SettingsPresenter::~SettingsPresenter()
{
    m_view.SetOnSaveClicked({});
    m_view.SetOnCancelClicked({});
    m_view.SetOnApiPageRequested({});
    m_view.SetApiStatusProvider({});
}

bool SettingsPresenter::ShowSettingsDialog(HINSTANCE hInstance, HWND parentHwnd, AppConfig &inOutConfig)
{
    m_workingConfig = m_initialConfig = inOutConfig;
    m_activeConfig = &inOutConfig;
    const bool accepted = m_view.ShowModal(hInstance, parentHwnd, m_workingConfig);
    m_activeConfig = nullptr;
    return accepted;
}

void SettingsPresenter::OnSaveClicked()
{
    if (SaveWorkingSettings())
        m_view.CloseModal(true);
}

bool SettingsPresenter::SaveWorkingSettings(bool apiOnly)
{
    if (!m_activeConfig || !m_view.TryReadFormValues(m_workingConfig, m_workingConfig))
        return false;
    // API commands can update playback while the modal dialog is open. Merge only
    // fields edited here instead of restoring an old snapshot over those changes.
    auto &current = *m_activeConfig;
#define MERGE_SETTING(field)                                                                                           \
    if (m_workingConfig.field != m_initialConfig.field)                                                                \
    current.field = m_workingConfig.field
    if (!apiOnly)
    {
        MERGE_SETTING(repeatEnabled);
        MERGE_SETTING(seekStepSeconds);
        MERGE_SETTING(volume);
        MERGE_SETTING(muted);
        MERGE_SETTING(speed);
        MERGE_SETTING(topMost);
        MERGE_SETTING(subtitleVisible);
        MERGE_SETTING(subtitleScale);
    }
    MERGE_SETTING(api);
#undef MERGE_SETTING
    m_configService.Save(current);
    if (apiOnly)
        m_initialConfig.api = m_workingConfig.api;
    else
        m_initialConfig = m_workingConfig;
    return true;
}

void SettingsPresenter::OnCancelClicked()
{
    m_view.CloseModal(false);
}
