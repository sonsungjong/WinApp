#include "Presenters/HomePresenter.h"

HomePresenter::HomePresenter(IHomeView& view, const HomeModel& model)
    : m_view(view), m_model(model)
{
}

void HomePresenter::Initialize()
{
    m_view.SetTitle(m_model.GetTitle());
}
