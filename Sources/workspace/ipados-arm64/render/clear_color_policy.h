#pragma once
#include <cstdint>
// sRGB converts RGB, not alpha. Both endpoints (0 and 1) are fixed points.
// Only these exact colors can keep the active target view when a D3D clear
// specifies unencoded bytes. All other colors retain the original path.
constexpr bool ICSMClearColorIsSRGBInvariant(std::uint32_t color) {
    for(unsigned shift=0;shift<24;shift+=8) {
        const unsigned component=(color>>shift)&255;
        if(component!=0 && component!=255)return false;
    }
    return true;
}
