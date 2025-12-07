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

    [[nodiscard]] inline uint32_t operator&(ColliderLayerMask a, ColliderLayerMask b) {
        return static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b);
    }

    [[nodiscard]] inline uint32_t operator|(ColliderLayerMask a, ColliderLayerMask b) {
        return static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b);
    }

    [[nodiscard]] inline ColliderLayerMask toLayerMask(std::uint32_t mask) {
        return static_cast<ColliderLayerMask>(mask);
    }

    struct CollisionManifold {
        bool colliding = false;
        Vector2 normal{0.0f, 0.0f};
        float depth = 0.0f;
        Vector2 contactPoint{0.0f, 0.0f};
    };

    enum class CollisionState {
        None,   // Not colliding (should not be used)
        Enter,  // First frame of collision
        Stay,   // Ongoing collision
        Exit    // Was colliding last frame, but not anymore
    };

    struct CollisionEvent {
        Collider* colliderA = nullptr;
        Collider* colliderB = nullptr;
        CollisionManifold manifold;
        CollisionState state = CollisionState::None;
        
        // For Box2D integration - store entity pointers when colliders are null
        class Entity* entityA = nullptr;
        class Entity* entityB = nullptr;
        
        // Helper to get the other entity in the collision (relative to 'self')
        [[nodiscard]] class Entity* getOtherEntity(const class Entity* self) const {
            if (colliderA && &colliderA->entity() == self) {
                return colliderB ? &colliderB->entity() : entityB;
            }
            if (colliderB && &colliderB->entity() == self) {
                return colliderA ? &colliderA->entity() : entityA;
            }
            if (entityA == self) return entityB;
            if (entityB == self) return entityA;
            return nullptr;
        }
    };

    using CollisionCallback = std::function<void(const CollisionEvent&)>;
}
