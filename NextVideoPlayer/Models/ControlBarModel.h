#pragma once

#include "framework.h"
#include "Models/IVideoModel.h"

struct ControlBarState
{
    PlaybackState playbackState = PlaybackState::Stopped;
    double position = 0.0;
    double duration = 0.0;
    int volume = 100;
    bool muted = false;
    double speed = 1.0;
    bool repeatEnabled = false;
    double abLoopA = -1.0;
    double abLoopB = -1.0;
    bool subtitleVisible = true;
    double subtitleDelay = 0.0;
    bool topMost = false;
    bool playlistOpen = false;
};

class ControlBarModel
{
public:
    ControlBarModel() = default;
    ~ControlBarModel() = default;

    // Updates internal state and returns true if visual representation changed.
    bool UpdateState(const ControlBarState& newState);
    const ControlBarState& GetState() const;

private:
    ControlBarState m_state{};
    bool m_initialized = false;
};

