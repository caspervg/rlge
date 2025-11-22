#pragma once
#include <cstdint>
#include <functional>

#include "raylib.h"

namespace rlge {
    class Collider;

    enum class ColliderType {
        Solid,
        Trigger,
        Sensor,
        Kinematic
    };

    enum class ColliderLayerMask : std::uint32_t {
        LAYER_WORLD  = 1u << 0,
        LAYER_PLAYER = 1u << 1,
        LAYER_ENEMY  = 1u << 2,
        LAYER_ITEM   = 1u << 3,
        LAYER_BULLET = 1u << 4,
    };

    [[nodiscard]] inline std::uint32_t operator&(ColliderLayerMask a, ColliderLayerMask b) {
        return static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b);
    }

    struct CollisionManifold {
        bool colliding = false;
        Vector2 normal{0.0f, 0.0f};
        float depth = 0.0f;
    };

    using CollisionCallback = std::function<void(const Collider*)>;
}
