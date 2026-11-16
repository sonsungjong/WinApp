#include "Presenters/SettingPresenter.h"

SettingPresenter::SettingPresenter(ISettingView& view, const SettingModel& model)
    : m_view(view), m_model(model)
{
    m_view.SetOnCloseRequested([this]() { m_view.Close(); });
}

SettingPresenter::~SettingPresenter()
{
    m_view.SetOnCloseRequested({});
}

void SettingPresenter::Show()
{
    m_view.SetTitle(m_model.GetTitle());
    m_view.ShowModal();
}
