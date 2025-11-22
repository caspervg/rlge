#pragma once
#include <stdexcept>

#include "raylib.h"
#include "../collider.hpp"
#include "../narrow_phase.hpp"

namespace rlge {
    class CollisionSystem;
    class Entity;

    class CircleCollider final : public Collider {
    public:
        CircleCollider(Entity& e,
                       CollisionSystem& system,
                       const ColliderType type,
                       const ColliderLayerMask layer,
                       const ColliderLayerMask mask,
                       const Vector2& center,
                       const float radius,
                       const bool trigger = true) :
            Collider(e, system, type, layer, mask, trigger), center_(center), radius_(radius) {}

        [[nodiscard]] CollisionManifold testAgainst(const Collider& other) const override {
            return other.collideWith(*this);
        }

        [[nodiscard]] CollisionManifold collideWith(const BoxCollider& b) const override {
            return narrow_phase::boxCircle(b, *this);
        }

        [[nodiscard]] CollisionManifold collideWith(const CircleCollider& c) const override {
            return narrow_phase::circleCircle(*this, c);
        }

        [[nodiscard]] CollisionManifold collideWith(const ObbCollider& o) const override {
            return narrow_phase::obbCircle(o, *this);
        }

        [[nodiscard]] CollisionManifold collideWith(const PolygonCollider& p) const override {
            return narrow_phase::polyCircle(p, *this);
        }

        [[nodiscard]] Rectangle axisAlignedWorldBounds() const override {
            auto const t = entity().get<Transform>();
            const auto x = t->position.x + center_.x;
            const auto y = t->position.y + center_.y;
            return {x - radius_, y - radius_, radius_ * 2, radius_ * 2};
        }

        [[nodiscard]] Vector2 center() const {
            auto* t = entity().get<Transform>();
            return t ? t->position + center_ : center_;
        }

        [[nodiscard]] float radius() const {
            auto* t = entity().get<Transform>();

            if (t && t->scale.x != t->scale.y) {
                throw std::runtime_error("CircleCollider: scale.x != scale.y. Ellipses are not supported.");
            }

            return t ? radius_ * t->scale.x : radius_;
        }

        [[nodiscard]] std::vector<Vector2> points() const override {
            const auto worldCenter = center();
            const auto worldRadius = radius();

            constexpr auto segments = 32;
            std::vector<Vector2> pts;
            pts.reserve(segments);

            for (auto i = 0; i < segments; ++i) {
                const float t = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(segments);
                const float cs = cosf(t);
                const float sn = sinf(t);
                pts.emplace_back(worldCenter.x + worldRadius * cs, worldCenter.y + worldRadius * sn);
            }

            return pts;
        }

    private:
        Vector2 center_;
        float radius_;
    };
}
