#pragma once
#include "collider_types.hpp"
#include "collision_system.hpp"
#include "entity.hpp"
#include "raylib.h"
#include "scene.hpp"
#include "../component.hpp"
#include "../transformer.hpp"

namespace {
    Color colorForLayer(const rlge::ColliderLayerMask layer) {
        using L = rlge::ColliderLayerMask;
        switch (layer) {
        case L::LAYER_WORLD:
            return GRAY;
        case L::LAYER_PLAYER:
            return GREEN;
        case L::LAYER_ENEMY:
            return RED;
        case L::LAYER_ITEM:
            return GOLD;
        case L::LAYER_BULLET:
            return PURPLE;
        default:
            return RAYWHITE;
        }
    }

    Color applyTriggerStyle(Color base, const bool isTrigger) {
        if (!isTrigger)
            return base;

        base.a = 128;
        return base;
    }
}

namespace rlge {
    class BoxCollider;
    class CircleCollider;
    class ObbCollider;
    class PolygonCollider;
    class CapsuleCollider;
    class CollisionSystem;

    class Collider : public Component {
    public:
        Collider(Entity& e,
                 CollisionSystem& system,
                 const ColliderType type,
                 const ColliderLayerMask layer,
                 const ColliderLayerMask mask,
                 const bool trigger = true) :
            Component(e)
            , system_(system)
            , type_(type)
            , layer_(layer)
            , mask_(mask)
            , trigger_(trigger) {
            system_.registerCollider(this);
        }

        ~Collider() override {
            system_.unregisterCollider(this);
        };

        void draw() override {
            if (!system_.debug())
                return;

            const auto shapeColor = applyTriggerStyle(colorForLayer(layer_), trigger_);
            constexpr Color aabbColor = {80, 80, 80, 255};

            const auto worldRect = axisAlignedWorldBounds();

            // Draw shape
            const auto worldPoints = points();
            entity().scene().rq().submitWorld([worldRect, worldPoints, shapeColor, aabbColor] {
                DrawRectangleLinesEx(worldRect, 1.0f, aabbColor);
                const auto n = worldPoints.size();
                if (n >= 2) {
                    for (size_t i = 0; i < n; ++i) {
                        const auto& p1 = worldPoints[i];
                        const auto& p2 = worldPoints[(i + 1) % n];
                        DrawLineEx(p1, p2, 2.0f, shapeColor);
                    }
                }
            });
        }

        [[nodiscard]] virtual Rectangle axisAlignedWorldBounds() const = 0; // for broad phase collision testing

        [[nodiscard]] virtual CollisionManifold testAgainst(const Collider& other) const = 0;

        [[nodiscard]] virtual CollisionManifold collideWith(const BoxCollider& b) const = 0;
        [[nodiscard]] virtual CollisionManifold collideWith(const CircleCollider& c) const = 0;
        [[nodiscard]] virtual CollisionManifold collideWith(const ObbCollider& o) const = 0;
        [[nodiscard]] virtual CollisionManifold collideWith(const PolygonCollider& p) const = 0;

        [[nodiscard]] virtual std::vector<Vector2> points() const = 0;

        [[nodiscard]] ColliderType type() const { return type_; }
        [[nodiscard]] ColliderLayerMask layer() const { return layer_; }
        [[nodiscard]] ColliderLayerMask mask() const { return mask_; }

        virtual void resolve(const CollisionManifold& m) {
            if (!m.colliding || trigger_)
                return;
            const auto t = entity().get<Transform>();
            t->position.x -= m.normal.x * m.depth * 0.5f;
            t->position.y -= m.normal.y * m.depth * 0.5f;
        };

        void setOnCollision(CollisionCallback cb) {
            onCollision_ = std::move(cb);
        }

        [[nodiscard]] CollisionCallback getOnCollision() const { return onCollision_; }

        void onCollision(const Collider* other) const {
            if (onCollision_) {
                onCollision_(other);
            }
        }

        [[nodiscard]] bool isTrigger() const { return trigger_; };

    private:
        CollisionSystem& system_;
        ColliderType type_;
        ColliderLayerMask layer_;
        ColliderLayerMask mask_;
        bool trigger_;
        CollisionCallback onCollision_;
    };
}
