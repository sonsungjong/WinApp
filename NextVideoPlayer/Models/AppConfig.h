#pragma once

#include "Models/ApiSettings.h"

struct AppConfig
{
    bool repeatEnabled = false;
    int seekStepSeconds = 5;
    int volume = 100;
    bool muted = false;
    double speed = 1.0;
    bool topMost = false;
    bool subtitleVisible = true;
    double subtitleScale = 1.0;
    ApiSettings api;
};

