#include "Presenters/MainPresenter.h"
#include "Presenters/HomePresenter.h"
#include "Presenters/SettingPresenter.h"
#include "Resource.h"

MainPresenter::MainPresenter(IMainView& view, const MainModel& model, HomePresenter& home, SettingPresenter& setting)
    : m_view(view), m_model(model), m_home(home), m_setting(setting)
{
}

MainPresenter::~MainPresenter()
{
    m_view.SetOnCommand({});
}

void MainPresenter::Initialize()
{
    m_view.SetWindowTitle(m_model.GetWindowTitle());
    m_view.SetOnCommand([this](int commandId) { HandleCommand(commandId); });
    m_home.Initialize();
}

void MainPresenter::HandleCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_SETTING:
        m_setting.Show();
        break;
    case IDM_ABOUT:
        m_view.ShowAbout();
        break;
    case IDM_EXIT:
        m_view.Close();
        break;
    }
}
