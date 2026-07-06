#pragma once
#include <cmath>
#include <cstdint>

// Deterministic, seedable value noise + fBM. Header-only, no state beyond the seed.
namespace vox::noise {

    inline std::uint32_t hash(std::uint32_t x) {
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }

    inline std::uint32_t hash3(const std::uint32_t seed, const int x, const int y, const int z) {
        std::uint32_t h = seed;
        h = hash(h ^ static_cast<std::uint32_t>(x) * 0x9e3779b1U);
        h = hash(h ^ static_cast<std::uint32_t>(y) * 0x85ebca77U);
        h = hash(h ^ static_cast<std::uint32_t>(z) * 0xc2b2ae3dU);
        return h;
    }

    // Uniform [0, 1)
    inline float rand01(const std::uint32_t seed, const int x, const int y, const int z = 0) {
        return static_cast<float>(hash3(seed, x, y, z) >> 8) / 16777216.0f;
    }

    inline float smooth(const float t) { return t * t * (3.0f - 2.0f * t); }

    inline float lerp(const float a, const float b, const float t) { return a + (b - a) * t; }

    // 2D value noise, output [0, 1)
    inline float value2(const std::uint32_t seed, const float x, const float y) {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const float tx = smooth(x - static_cast<float>(xi));
        const float ty = smooth(y - static_cast<float>(yi));
        const float c00 = rand01(seed, xi, yi);
        const float c10 = rand01(seed, xi + 1, yi);
        const float c01 = rand01(seed, xi, yi + 1);
        const float c11 = rand01(seed, xi + 1, yi + 1);
        return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
    }

    // 3D value noise, output [0, 1)
    inline float value3(const std::uint32_t seed, const float x, const float y, const float z) {
        const int xi = static_cast<int>(std::floor(x));
        const int yi = static_cast<int>(std::floor(y));
        const int zi = static_cast<int>(std::floor(z));
        const float tx = smooth(x - static_cast<float>(xi));
        const float ty = smooth(y - static_cast<float>(yi));
        const float tz = smooth(z - static_cast<float>(zi));
        const float c000 = rand01(seed, xi, yi, zi);
        const float c100 = rand01(seed, xi + 1, yi, zi);
        const float c010 = rand01(seed, xi, yi + 1, zi);
        const float c110 = rand01(seed, xi + 1, yi + 1, zi);
        const float c001 = rand01(seed, xi, yi, zi + 1);
        const float c101 = rand01(seed, xi + 1, yi, zi + 1);
        const float c011 = rand01(seed, xi, yi + 1, zi + 1);
        const float c111 = rand01(seed, xi + 1, yi + 1, zi + 1);
        const float a = lerp(lerp(c000, c100, tx), lerp(c010, c110, tx), ty);
        const float b = lerp(lerp(c001, c101, tx), lerp(c011, c111, tx), ty);
        return lerp(a, b, tz);
    }

    inline float fbm2(const std::uint32_t seed, float x, float y, const int octaves) {
        float sum = 0.0f;
        float amp = 0.5f;
        float total = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += value2(seed + static_cast<std::uint32_t>(i) * 101U, x, y) * amp;
            total += amp;
            amp *= 0.5f;
            x *= 2.02f;
            y *= 2.02f;
        }
        return sum / total;
    }

    inline float fbm3(const std::uint32_t seed, float x, float y, float z, const int octaves) {
        float sum = 0.0f;
        float amp = 0.5f;
        float total = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            sum += value3(seed + static_cast<std::uint32_t>(i) * 131U, x, y, z) * amp;
            total += amp;
            amp *= 0.5f;
            x *= 2.02f;
            y *= 2.02f;
            z *= 2.02f;
        }
        return sum / total;
    }

} // namespace vox::noise
