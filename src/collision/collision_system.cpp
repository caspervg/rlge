#include "collision_system.hpp"

#include <print>

#include "collider.hpp"
#include "entity.hpp"
#include "imgui.h"
#include "physics_body.hpp"
#include "raylib.h"
#include "raymath.h"
#include "scene.hpp"

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
        std::vector<CollisionEvent> newCollisions;
        collisionEvents_.clear();
        collisionPairsThisFrame_ = {};
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

                const auto m = a->testAgainst(*b);
                if (!m.colliding)
                    // Narrow phase collision check does not succeed
                    continue;

                // Collision detected
                CollisionPair cp = {a, b};
                if (collisionPairsThisFrame_.contains(cp)) {
                    continue;   // Already processed
                }
                collisionPairsThisFrame_.insert(cp);

                const auto wasColliding = collisionPairsLastFrame_.contains(cp);
                const auto state = wasColliding ? CollisionState::Stay : CollisionState::Enter;

                newCollisions.push_back({a, b, m, state});
            }
        }

        for (const auto& lastPair : collisionPairsLastFrame_) {
            if (!collisionPairsThisFrame_.contains(lastPair)) {
                newCollisions.push_back({lastPair.a, lastPair.b, {}, CollisionState::Exit});
            }
        }

        collisionEvents_ = std::move(newCollisions);
        collisionPairsLastFrame_ = std::move(collisionPairsThisFrame_);

        updating_ = false;
        flushPendingRemovals_();
    }

    const std::vector<CollisionEvent>& CollisionSystem::collisionEvents() const {
        return collisionEvents_;
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

            // Remove all pairs involving this collider from last frame's state
            std::erase_if(collisionPairsLastFrame_, [c](const CollisionPair& pair) {
                return pair.a == c || pair.b == c;
            });
        }
        pendingRemovals_.clear();
        compact_();
    }

    void CollisionResponseSystem::addHandler(CollisionResponseHandler handler) {
        handlers_.push_back(std::move(handler));
    }

    void CollisionResponseSystem::update(Scene& scene) {
        auto& collisions = scene.collisions().collisionEvents();

        for (const auto& event : collisions) {
            // Resolve potential penetration
            resolve_(event.colliderA, event.colliderB, event.manifold);

            // Process collider A
            processEntity_(event.colliderA->entity(), event);

            // Process collider B, with flipped normal
            CollisionEvent flipped = event;
            flipped.manifold.normal = Vector2Negate(flipped.manifold.normal);
            processEntity_(event.colliderB->entity(), flipped);
        }
    }

    void CollisionResponseSystem::processEntity_(Entity& entity, const CollisionEvent& event) {
        // Always handle PhysicsBody if present
        if (auto* physics = entity.get<PhysicsBody>()) {
            physics->onCollision(event);
        }

        for (const auto& handler : handlers_) {
            handler(entity, event);
        }
    }

    void CollisionResponseSystem::resolve_(Collider* a, Collider* b, const CollisionManifold& manifold) {
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
            b->resolve(manifold);
            return;
        }

        if (kinB && solidA && !triggerA) {
            CollisionManifold ma = manifold;
            ma.normal = Vector2Negate(ma.normal);
            a->resolve(ma);
            return;
        }

        // Solid vs. solid: symmetric resolution as before.
        if (solidA && solidB) {
            std::println("solidA vs solidB");
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
