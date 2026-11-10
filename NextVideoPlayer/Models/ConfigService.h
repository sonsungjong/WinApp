#pragma once

#include "framework.h"
#include "Models/AppConfig.h"

class ConfigService
{
public:
    ConfigService();
    explicit ConfigService(std::wstring configPath, std::wstring legacyConfigPath = {});
    ~ConfigService() = default;

    AppConfig Load() const;
    void Save(const AppConfig& config) const;
    const std::wstring& GetConfigPath() const;

private:
    std::wstring m_configPath;
    std::wstring m_legacyConfigPath;
};
