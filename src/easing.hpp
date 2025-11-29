#pragma once
// Simple easing helpers (Robert Penner style).
// Input t in [0,1], returns eased value in [0,1].

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace rlge {

    inline float clamp01(float t) { return std::clamp(t, 0.0f, 1.0f); }

    // Linear
    inline float easeLinear(float t) { return clamp01(t); }

    // Quadratic
    inline float easeInQuad(float t) {
        t = clamp01(t);
        return t * t;
    }
    inline float easeOutQuad(float t) {
        t = clamp01(t);
        return t * (2.0f - t);
    }
    inline float easeInOutQuad(float t) {
        t = clamp01(t);
        if (t < 0.5f) return 2.0f * t * t;
        return -1.0f + (4.0f - 2.0f * t) * t;
    }

    // Back easing (overshoot)
    inline float easeOutBack(float t) {
        constexpr auto c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        t = clamp01(t);
        return 1.0f + c3 * std::pow(t - 1.0f, 3) + c1 * std::pow(t - 1.0f, 2);
    }
    inline float easeInBack(float t) {
        constexpr float c1 = 1.70158f;
        t = clamp01(t);
        return t * t * ((c1 + 1.0f) * t - c1);
    }

    // Elastic-ish ease-out
    inline float easeOutElastic(float t) {
        t = clamp01(t);
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        constexpr auto p = 0.3f;
        return std::pow(2.0f, -10.0f * t) *
               std::sin((t - p / 4.0f) * (2.0f * PI) / p) + 1.0f;
    }

} // namespace rlge