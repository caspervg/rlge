#pragma once
#include <vector>

#include "raylib.h"
#include "raymath.h"

#include "../collider.hpp"
#include "../narrow_phase.hpp"

namespace rlge {
    class BoxCollider;
    class CircleCollider;
    class ObbCollider;
    class CapsuleCollider;
    class CollisionSystem;
    class Entity;

    class PolygonCollider final : public Collider {
    public:
        PolygonCollider(Entity& e,
                        CollisionSystem& system,
                        const ColliderType type,
                        const ColliderLayerMask layer,
                        const ColliderLayerMask mask,
                        std::vector<Vector2> localPoints,
                        const bool trigger = true)
            : Collider(e, system, type, layer, mask, trigger)
            , localPoints_(std::move(localPoints)) {}

        [[nodiscard]] CollisionManifold testAgainst(const Collider& other) const override {
            return other.collideWith(*this);
        }

        [[nodiscard]] CollisionManifold collideWith(const BoxCollider& b) const override {
            return narrow_phase::polyBox(*this, b);
        }

        [[nodiscard]] CollisionManifold collideWith(const CircleCollider& c) const override {
            return narrow_phase::polyCircle(*this, c);
        }

        [[nodiscard]] CollisionManifold collideWith(const ObbCollider& o) const override {
            return narrow_phase::obbPolygon(o, *this);
        }

        [[nodiscard]] CollisionManifold collideWith(const PolygonCollider& p) const override {
            return narrow_phase::polyPoly(*this, p);
        }

        [[nodiscard]] Rectangle axisAlignedWorldBounds() const override {
            const auto worldPts = points();
            if (worldPts.empty()) {
                return Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
            }

            float minX = worldPts[0].x;
            float minY = worldPts[0].y;
            float maxX = worldPts[0].x;
            float maxY = worldPts[0].y;

            for (const auto& p : worldPts) {
                if (p.x < minX) minX = p.x;
                if (p.x > maxX) maxX = p.x;
                if (p.y < minY) minY = p.y;
                if (p.y > maxY) maxY = p.y;
            }

            return Rectangle{minX, minY, maxX - minX, maxY - minY};
        }

        [[nodiscard]] std::vector<Vector2> points() const override {
            std::vector<Vector2> worldPts;
            worldPts.reserve(localPoints_.size());

            auto* t = entity().get<Transform>();
            if (!t) {
                return localPoints_;
            }

            for (const auto& p : localPoints_) {
                const Vector2 scaled{p.x * t->scale.x, p.y * t->scale.y};
                const Vector2 rotated = Vector2Rotate(scaled, t->rotation);
                const Vector2 world = Vector2Add(t->position, rotated);
                worldPts.push_back(world);
            }

            return worldPts;
        }

    private:
        std::vector<Vector2> localPoints_;
    };

} // namespace rlge
