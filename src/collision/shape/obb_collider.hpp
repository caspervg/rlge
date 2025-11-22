#pragma once
#include <vector>

#include "raylib.h"
#include "raymath.h"

#include "entity.hpp"
#include "../collider.hpp"
#include "../narrow_phase.hpp"

namespace rlge {
    class Collider;
    class CollisionSystem;
    class Entity;

    class ObbCollider final : public Collider {
    public:
        ObbCollider(Entity& e,
                    CollisionSystem& system,
                    const ColliderType type,
                    const ColliderLayerMask layer,
                    const ColliderLayerMask mask,
                    const Vector2& center,
                    const Vector2& halfSize,
                    const float rotation,
                    const bool trigger = true) :
            Collider(e, system, type, layer, mask, trigger)
            , center_(center)
            , halfSize_(halfSize)
            , rotation_(rotation) {}

        [[nodiscard]] CollisionManifold testAgainst(const Collider& other) const override {
            return other.collideWith(*this);
        }

        [[nodiscard]] CollisionManifold collideWith(const BoxCollider& b) const override {
            return narrow_phase::boxObb(*this, b);
        }

        [[nodiscard]] CollisionManifold collideWith(const CircleCollider& c) const override {
            return narrow_phase::obbCircle(*this, c);
        }

        [[nodiscard]] CollisionManifold collideWith(const PolygonCollider& p) const override {
            return narrow_phase::obbPolygon(*this, p);
        }

        [[nodiscard]] CollisionManifold collideWith(const ObbCollider& o) const override {
            return narrow_phase::obbObb(*this, o);
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
            worldPts.reserve(4);

            auto* t = entity().get<Transform>();
            if (!t) {
                worldPts.push_back(Vector2Add(center_, {-halfSize_.x, -halfSize_.y}));
                worldPts.push_back(Vector2Add(center_, { halfSize_.x, -halfSize_.y}));
                worldPts.push_back(Vector2Add(center_, { halfSize_.x,  halfSize_.y}));
                worldPts.push_back(Vector2Add(center_, {-halfSize_.x,  halfSize_.y}));
                return worldPts;
            }

            const Vector2 localCorners[4] = {
                {-halfSize_.x, -halfSize_.y},
                { halfSize_.x, -halfSize_.y},
                { halfSize_.x,  halfSize_.y},
                {-halfSize_.x,  halfSize_.y}
            };

            for (const auto& c : localCorners) {
                // Apply local OBB rotation around its center
                const Vector2 rotatedLocal = Vector2Rotate(c, rotation_);
                const Vector2 localPoint = Vector2Add(center_, rotatedLocal);

                // Then apply entity scale, rotation, translation (same order as PolygonCollider / BoxCollider)
                const Vector2 scaled{localPoint.x * t->scale.x, localPoint.y * t->scale.y};
                const Vector2 rotatedWorld = Vector2Rotate(scaled, t->rotation);
                const Vector2 world = Vector2Add(t->position, rotatedWorld);
                worldPts.push_back(world);
            }

            return worldPts;
        }

    private:
        Vector2 center_;
        Vector2 halfSize_;
        float rotation_;
    };
}
