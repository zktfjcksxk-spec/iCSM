#pragma once
#include <cmath>

// Count only foreground display time. Lifecycle notifications account for
// the last active interval before pausing; a background wait never skips it.
struct ICSMAnnouncementClock {
    static constexpr double requiredSeconds = 10.0;
    double elapsed = 0, last = 0;
    bool started = false, active = false;
    void begin(double now, bool foreground) {
        elapsed = 0; last = now; started = true; active = foreground;
    }
    void advance(double now) {
        if (!started || !std::isfinite(now) || now < last) return;
        if (active) elapsed += now - last;
        last = now;
    }
    void setActive(double now, bool foreground) {
        advance(now); active = foreground;
    }
    bool complete() const { return started && elapsed >= requiredSeconds; }
};
