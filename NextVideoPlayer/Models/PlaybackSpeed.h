#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace PlaybackSpeed
{
    inline constexpr double kMinimum = 0.20;
    inline constexpr double kMaximum = 16.00;

    inline constexpr std::array<double, 12> kPresets{
        kMinimum, 0.25, 0.50, 0.75, 1.00, 1.25,
        1.50, 2.00, 3.00, 4.00, 8.00, kMaximum
    };

    inline double Clamp(double speed)
    {
        return std::isfinite(speed) ? std::clamp(speed, kMinimum, kMaximum) : 1.0;
    }

    inline double Step(double speed, int direction)
    {
        speed = Clamp(speed);
        if (direction > 0)
        {
            const auto next = std::upper_bound(kPresets.begin(), kPresets.end(), speed);
            return next != kPresets.end() ? *next : kMaximum;
        }
        if (direction < 0)
        {
            const auto next = std::lower_bound(kPresets.begin(), kPresets.end(), speed);
            return next != kPresets.begin() ? *(next - 1) : kMinimum;
        }
        return speed;
    }
}
