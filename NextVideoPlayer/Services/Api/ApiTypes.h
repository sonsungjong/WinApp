#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Api
{
inline constexpr std::size_t MaxMessageBytes = 1024 * 1024;
inline constexpr std::size_t MaxPendingCommands = 128;

struct Command
{
    std::string requestId;
    std::optional<std::vector<std::string>> files;
    std::optional<int> playlistIndex;
    std::optional<std::string> playback;
    std::optional<double> positionMs;
    std::optional<double> speed;
    std::optional<int> volume;
    std::optional<bool> muted, repeat, fullscreen, topMost, subscribe;
    std::optional<std::int64_t> executeAtMs;
    bool HasChanges() const
    {
        return files || playlistIndex || playback || positionMs || speed || volume || muted || repeat || fullscreen ||
               topMost;
    }
};

struct State
{
    std::string instanceId;
    std::string playback = "stopped";
    std::string file;
    std::string mediaError;
    bool ready = false, loading = false;
    double positionMs = 0, durationMs = 0, speed = 1;
    int volume = 100, playlistIndex = -1, playlistCount = 0;
    bool muted = false, repeat = false, fullscreen = false, topMost = false;
    std::vector<std::string> files;
    std::uint64_t playlistRevision = 0;
    std::int64_t timestampMs = 0;
};

struct Result
{
    bool ok = true;
    bool accepted = false;
    std::string code, message;
};
} // namespace Api
