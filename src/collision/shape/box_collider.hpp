#pragma once
#include <vector>

#include "../collider.hpp"
#include "../narrow_phase.hpp"

namespace rlge {
    class CapsuleCollider;
    class PolygonCollider;
    class ObbCollider;
    class CircleCollider;
    class Collider;
    class CollisionSystem;
    class Entity;

    class BoxCollider final : public Collider {
    public:
        BoxCollider(Entity& e,
                    CollisionSystem& system,
                    const ColliderType type,
                    const ColliderLayerMask layer,
                    const ColliderLayerMask mask,
                    const Rectangle& local,
                    const bool trigger = true) :
            Collider(e, system, type, layer, mask, trigger), local_(local) {}


        [[nodiscard]] CollisionManifold testAgainst(const Collider& other) const override {
            return other.collideWith(*this);
        }

        [[nodiscard]] CollisionManifold collideWith(const BoxCollider& b) const override {
            return narrow_phase::boxBox(*this, b);
        }

        [[nodiscard]] CollisionManifold collideWith(const CircleCollider& c) const override {
            return narrow_phase::boxCircle(*this, c);
        }

        [[nodiscard]] CollisionManifold collideWith(const ObbCollider& o) const override {
            return narrow_phase::boxObb(o, *this);
        }

        [[nodiscard]] CollisionManifold collideWith(const PolygonCollider& p) const override {
            return narrow_phase::polyBox(p, *this);
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
                worldPts.push_back({local_.x, local_.y});
                worldPts.push_back({local_.x + local_.width, local_.y});
                worldPts.push_back({local_.x + local_.width, local_.y + local_.height});
                worldPts.push_back({local_.x, local_.y + local_.height});
                return worldPts;
            }

            const Vector2 corners[4] = {
                {local_.x, local_.y},
                {local_.x + local_.width, local_.y},
                {local_.x + local_.width, local_.y + local_.height},
                {local_.x, local_.y + local_.height}
            };

            for (const auto& p : corners) {
                const Vector2 scaled{p.x * t->scale.x, p.y * t->scale.y};
                const Vector2 rotated = Vector2Rotate(scaled, t->rotation);
                const Vector2 world = Vector2Add(t->position, rotated);
                worldPts.push_back(world);
            }

            return worldPts;
        }

    private:
        Rectangle local_;
    };

} // rlge
