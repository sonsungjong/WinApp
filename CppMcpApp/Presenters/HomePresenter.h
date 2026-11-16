#pragma once

#include "Models/HomeModel.h"
#include "Views/IHomeView.h"

// Home-screen behavior belongs here. MainPresenter only coordinates the shell.
class HomePresenter
{
public:
    HomePresenter(IHomeView& view, const HomeModel& model);
    HomePresenter(const HomePresenter&) = delete;
    HomePresenter& operator=(const HomePresenter&) = delete;

    void Initialize();

private:
    IHomeView& m_view;
    const HomeModel& m_model;
};
