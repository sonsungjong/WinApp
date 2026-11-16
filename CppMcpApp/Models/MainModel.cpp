#include "Models/MainModel.h"
#include <utility>

MainModel::MainModel(std::wstring windowTitle)
    : m_windowTitle(std::move(windowTitle))
{
}

const std::wstring& MainModel::GetWindowTitle() const
{
    return m_windowTitle;
}
