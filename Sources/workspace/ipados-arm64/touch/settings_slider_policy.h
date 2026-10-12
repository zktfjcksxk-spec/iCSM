#pragma once
#include <algorithm>
#include <cmath>

namespace ICSMSettingsTouch {
enum Intent { Pending, Vertical, HorizontalSlider, Ignore };
inline Intent Classify(double dx, double dy, bool slider, Intent previous = Pending) {
    if (previous != Pending) return previous;
    if ((std::max)(std::abs(dx), std::abs(dy)) < 6) return Pending;
    if (std::abs(dy) > std::abs(dx)) return Vertical;
    return slider ? HorizontalSlider : Ignore;
}
inline bool Hit(double x, double y, double left, double top, double width, double height) {
    if (!std::isfinite(x) || !std::isfinite(y) || width <= 0 || height <= 0) return false;
    const double extraY = (std::max)(0.0, (44.0 - height) / 2.0);
    return x >= left - 8 && x <= left + width + 8 && y >= top - extraY && y <= top + height + extraY;
}
inline float Fraction(double x, double left, double width, double thumbWidth) {
    if (!std::isfinite(x) || width <= thumbWidth) return 0;
    return float((std::max)(0.0, (std::min)(1.0, (x - left - thumbWidth / 2) / (width - thumbWidth))));
}
inline bool OnThumb(double x, double left, double width, double thumbWidth, float value) {
    const double center = left + thumbWidth / 2 + value * (width - thumbWidth);
    return std::abs(x - center) <= thumbWidth / 2 + 8;
}
inline float DragFraction(double x, double originX, double left, double width, double thumbWidth,
                          float initialValue, bool grabbedThumb) {
    if (!grabbedThumb) return Fraction(x, left, width, thumbWidth);
    if (!std::isfinite(x) || width <= thumbWidth) return initialValue;
    return float((std::max)(0.0, (std::min)(1.0, initialValue + (x - originX) / (width - thumbWidth))));
}
}
