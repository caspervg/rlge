#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "raylib.h"

namespace anim_demo {
    enum class AnimState {
        Idle = 0,
        Walk = 1,
        Attack = 2,
        Hurt = 3,
        Death = 4
    };

    constexpr int kFrameW = 48;
    constexpr int kFrameH = 48;
    constexpr float kScale = 3.0f;
    inline constexpr std::array<int, 6> kVariants{{1, 2, 3, 4, 5, 6}};

    inline std::string clipPath(const int variant, const char* file) {
        return "assets/" + std::to_string(variant) + "/" + file;
    }

    struct FlashParams {
        float intensity = 0.0f;
        Vector3 flashColor = {1.0f, 1.0f, 1.0f};
    };

    struct WaterParams {
        float time = 0.0f;
        Vector2 resolution = {0.0f, 0.0f};
    };

    inline std::filesystem::path findDemoRoot() {
        const auto cwd = std::filesystem::current_path();
        const auto parentCandidate = cwd.parent_path() / "examples" / "anim_2d";
        if (std::filesystem::exists(parentCandidate)) {
            return parentCandidate;
        }
        return cwd;
    }
}
