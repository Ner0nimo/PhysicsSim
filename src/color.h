#pragma once

#include <cstdint>

namespace phys {

// Honest: hue in [0,1) -> 0xRRGGBB.
inline std::uint32_t hsvToRgb(float h, float s, float v) {
    float r, g, b;
    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - static_cast<float>(i);
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    auto to8 = [](float c) { return static_cast<std::uint32_t>(c * 255.0f) & 0xFFu; };
    return (to8(r) << 16) | (to8(g) << 8) | to8(b);
}

} // namespace phys
