#pragma once

#include "Models/MainModel.h"
#include "Views/IMainView.h"

class HomePresenter;
class SettingPresenter;

// Coordinates the shell; Home and Setting presenters own their screen behavior.
class MainPresenter
{
public:
    MainPresenter(IMainView& view, const MainModel& model, HomePresenter& home, SettingPresenter& setting);
    ~MainPresenter();
    MainPresenter(const MainPresenter&) = delete;
    MainPresenter& operator=(const MainPresenter&) = delete;

    void Initialize();

private:
    void HandleCommand(int commandId);

    IMainView& m_view;
    const MainModel& m_model;
    HomePresenter& m_home;
    SettingPresenter& m_setting;
};
