#pragma once

#include "Models/SettingModel.h"
#include "Views/ISettingView.h"

// Settings behavior belongs here, not in MainPresenter or HomePresenter.
class SettingPresenter
{
public:
    SettingPresenter(ISettingView& view, const SettingModel& model);
    ~SettingPresenter();
    SettingPresenter(const SettingPresenter&) = delete;
    SettingPresenter& operator=(const SettingPresenter&) = delete;

    void Show();

private:
    ISettingView& m_view;
    const SettingModel& m_model;
};
