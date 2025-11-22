#include "collision_system.hpp"
#include "collider.hpp"
#include "imgui.h"
#include "raylib.h"
#include "raymath.h"

namespace rlge {

    void CollisionSystem::registerCollider(Collider* c) {
        colliders_.push_back(c);
    }

    void CollisionSystem::unregisterCollider(Collider* c) {
        if (updating_) {
            pendingRemovals_.push_back(c);
            return;
        }
        std::erase(colliders_, c);
        compact_();
    }

    void CollisionSystem::update(float) {
        flushPendingRemovals_();
        updating_ = true;
        const size_t n = colliders_.size();
        for (size_t i = 0; i < n; ++i) {
            Collider* a = colliders_[i];
            if (!a)
                continue;

            for (size_t j = i + 1; j < n; ++j) {
                Collider* b = colliders_[j];
                if (!b)
                    continue;

                if ((a->layer() & b->mask()) == 0 || (b->layer() & a->mask()) == 0)
                    // Layer masks do not align, no collision check needed
                    continue;

                if (!CheckCollisionRecs(a->axisAlignedWorldBounds(), b->axisAlignedWorldBounds()))
                    // Broad phase collision check does not succeed, no need for narrow phase.
                    continue;

                auto m = a->testAgainst(*b);
                if (!m.colliding)
                    // Narrow phase collision check does not succeed
                    continue;

                a->onCollision(b);
                b->onCollision(a);

                resolve_(a, b, m);
            }
        }
        updating_ = false;
        flushPendingRemovals_();
    }

    void CollisionSystem::setDebug(const bool debug) {
        debug_ = debug;
    }

    bool CollisionSystem::debug() const { return debug_; }

    void CollisionSystem::debugOverlay() {
        if (colliders_.empty())
            return; // Nothing to see here

        if (ImGui::Begin("Collisions")) {
            ImGui::Checkbox("Draw colliders", &debug_);
        }
        ImGui::End();
    }

    void CollisionSystem::compact_() {
        std::erase(colliders_, nullptr);
    }

    void CollisionSystem::flushPendingRemovals_() {
        if (pendingRemovals_.empty())
            return;
        for (auto* c : pendingRemovals_) {
            std::erase(colliders_, c);
        }
        pendingRemovals_.clear();
        compact_();
    }

    void CollisionSystem::resolve_(Collider* a, Collider* b, const CollisionManifold& manifold) {
        const auto typeA = a->type();
        const auto typeB = b->type();

        const bool triggerA = a->isTrigger();
        const bool triggerB = b->isTrigger();

        // If both are triggers, nothing to resolve.
        if (triggerA && triggerB)
            return;

        const bool solidA = (typeA == ColliderType::Solid);
        const bool solidB = (typeB == ColliderType::Solid);
        const bool kinA   = (typeA == ColliderType::Kinematic);
        const bool kinB   = (typeB == ColliderType::Kinematic);

        // Static / kinematic vs. solid: move only the solid collider fully out of penetration.
        if (kinA && solidB && !triggerB) {
            CollisionManifold mb = manifold;
            // For B we normally flip the normal; do the same here but double the depth
            mb.normal = Vector2Negate(mb.normal);
            mb.depth *= 2.0f; // default resolve moves by depth * 0.5
            b->resolve(mb);
            return;
        }

        if (kinB && solidA && !triggerA) {
            CollisionManifold ma = manifold;
            ma.depth *= 2.0f; // default resolve moves by depth * 0.5
            a->resolve(ma);
            return;
        }

        // Solid vs. solid: symmetric resolution as before.
        if (solidA && solidB) {
            if (!triggerA) {
                a->resolve(manifold);
            }
            if (!triggerB) {
                CollisionManifold flipped = manifold;
                flipped.normal = Vector2Negate(flipped.normal);
                b->resolve(flipped);
            }
        }

        // Other combinations (sensors, triggers vs solids, kinematic vs kinematic)
        // are left to user code via callbacks only.
    }

}
