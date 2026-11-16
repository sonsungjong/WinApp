#pragma once

#include <string>

class IHomeView
{
public:
    virtual ~IHomeView() = default;

    virtual void SetTitle(const std::wstring& title) = 0;
};
