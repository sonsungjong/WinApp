#include "Presenters/SettingsPresenter.h"

SettingsPresenter::SettingsPresenter(ISettingsView& view, ConfigService& configService)
    : m_view(view)
    , m_configService(configService)
{
    m_view.SetOnSaveClicked([this]() { OnSaveClicked(); });
    m_view.SetOnCancelClicked([this]() { OnCancelClicked(); });
}

SettingsPresenter::~SettingsPresenter()
{
    m_view.SetOnSaveClicked({});
    m_view.SetOnCancelClicked({});
}

bool SettingsPresenter::ShowSettingsDialog(HINSTANCE hInstance, HWND parentHwnd, AppConfig& inOutConfig)
{
    m_workingConfig = inOutConfig;
    if (m_view.ShowModal(hInstance, parentHwnd, m_workingConfig))
    {
        inOutConfig = m_workingConfig;
        m_configService.Save(inOutConfig);
        return true;
    }
    return false;
}

void SettingsPresenter::OnSaveClicked()
{
    m_workingConfig = m_view.ReadFormValues(m_workingConfig);
    m_view.CloseModal(true);
}

void SettingsPresenter::OnCancelClicked()
{
    m_view.CloseModal(false);
}
