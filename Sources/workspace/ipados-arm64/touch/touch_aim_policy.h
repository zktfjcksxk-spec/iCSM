#pragma once
#include <algorithm>
#include <cmath>

namespace ICSMTouchAim {
enum Scope { Unscoped = 0, RifleScope = 1, SniperScope = 2 };
struct Settings {
    int mode;
    float total, rifle, sniper;
};
struct Delta {
    float yaw, pitch, acceleration, sensitivity;
};
inline float Sensitivity(float value) {
    return std::isfinite(value) ? (std::max)(0.1f, (std::min)(3.0f, value)) : 1.0f;
}
inline Scope SelectScope(bool scoped, bool rifle, bool sniper) {
    return !scoped ? Unscoped : sniper ? SniperScope : rifle ? RifleScope : Unscoped;
}
inline Delta Apply(float dx, float dy, double interval, float yawGain, float pitchGain,
                   const Settings &settings, Scope scope) {
    float sensitivity = Sensitivity(settings.total);
    if (scope == RifleScope) sensitivity *= Sensitivity(settings.rifle);
    if (scope == SniperScope) sensitivity *= Sensitivity(settings.sniper);
    Delta result = {0, 0, 1, sensitivity};
    if (!std::isfinite(dx) || !std::isfinite(dy) ||
        !std::isfinite(yawGain) || !std::isfinite(pitchGain)) return result;
    if (settings.mode == 1 && std::isfinite(interval) && interval >= 0.0001 && interval <= 0.25) {
        // Real UIKit points/second, independent of draw resolution and frame cap.
        // Smoothly rise from 1x at 120 pt/s to 2x at 720 pt/s. No history,
        // smoothing or inertia: release/re-entry cannot carry an old velocity.
        const double speed = std::hypot(double(dx), double(dy)) / interval;
        const double t = (std::max)(0.0, (std::min)(1.0, (speed - 120.0) / 600.0));
        result.acceleration = float(1.0 + t * t * (3.0 - 2.0 * t));
    }
    result.yaw = (-dx * yawGain) * sensitivity * result.acceleration;
    result.pitch = (dy * pitchGain) * sensitivity * result.acceleration;
    return result;
}
}
