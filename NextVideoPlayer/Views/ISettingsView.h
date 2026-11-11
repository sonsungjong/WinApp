#pragma once

#include "framework.h"
#include "Models/AppConfig.h"

class ISettingsView
{
public:
    virtual ~ISettingsView() = default;

    virtual bool ShowModal(HINSTANCE hInstance, HWND parentHwnd, const AppConfig& initialConfig) = 0;
    virtual AppConfig ReadFormValues(const AppConfig& baseConfig) const = 0;
    virtual bool TryReadFormValues(const AppConfig& baseConfig, AppConfig& result)
    {
        result = ReadFormValues(baseConfig);
        return true;
    }
    virtual void SetApiStatusProvider(std::function<std::wstring()> provider)
    {
    }
    virtual void SetOnApiPageRequested(std::function<void()> handler)
    {
    }
    virtual void CloseModal(bool accepted) = 0;

    virtual void SetOnSaveClicked(std::function<void()> handler) = 0;
    virtual void SetOnCancelClicked(std::function<void()> handler) = 0;
};

