#include "Models/ControlBarModel.h"
#include <cmath>

bool ControlBarModel::UpdateState(const ControlBarState& newState)
{
    bool visualChanged = !m_initialized ||
        (m_state.playbackState != newState.playbackState) ||
        (static_cast<int>(m_state.position * 5.0) != static_cast<int>(newState.position * 5.0)) ||
        (static_cast<int>(m_state.duration * 5.0) != static_cast<int>(newState.duration * 5.0)) ||
        (m_state.volume != newState.volume) ||
        (m_state.muted != newState.muted) ||
        (std::abs(m_state.speed - newState.speed) > 0.005) ||
        (m_state.repeatEnabled != newState.repeatEnabled) ||
        (std::abs(m_state.abLoopA - newState.abLoopA) > 0.005) ||
        (std::abs(m_state.abLoopB - newState.abLoopB) > 0.005) ||
        (m_state.subtitleVisible != newState.subtitleVisible) ||
        (m_state.playlistOpen != newState.playlistOpen);

    m_state = newState;
    m_initialized = true;
    return visualChanged;
}

const ControlBarState& ControlBarModel::GetState() const
{
    return m_state;
}

