#pragma once
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "component.hpp"
#include "entity.hpp"
#include "raylib.h"
#include "transformer.hpp"

namespace rlge {
    // Forward declarations
    class Collider;

    class CollisionSystem {
    public:
        void registerCollider(Collider* c);
        void unregisterCollider(Collider* c);
        void update(float);

    private:
        std::vector<Collider*> colliders_;
    };

    class Collider : public Component {
    public:
        using Callback = std::function<void(Collider&)>;

        Collider(Entity& e,
                 CollisionSystem& system,
                 Rectangle localBounds,
                 bool isTrigger = true) :
            Component(e),
            system_(system),
            local_(localBounds),
            trigger_(isTrigger) {
            system_.registerCollider(this);
        }

        ~Collider() override {
            system_.unregisterCollider(this);
        }

        Rectangle worldBounds() const {
            Rectangle r = local_;
            if (auto* t = entity().get<Transform>()) {
                r.x += t->position.x;
                r.y += t->position.y;
            }
            return r;
        }

        void setOnCollision(Callback cb) { onCollision_ = std::move(cb); }
        bool isTrigger() const { return trigger_; }

    private:
        friend class CollisionSystem;

        CollisionSystem& system_;
        Rectangle local_;
        bool trigger_;
        Callback onCollision_;
    };

    inline void CollisionSystem::registerCollider(Collider* c) {
        colliders_.push_back(c);
    }

    inline void CollisionSystem::unregisterCollider(Collider* c) {
        for (size_t i = 0; i < colliders_.size(); ++i) {
            if (colliders_[i] == c) {
                colliders_[i] = colliders_.back();
                colliders_.pop_back();
                return;
            }
        }
    }

    inline void CollisionSystem::update(float) {
        const size_t n = colliders_.size();
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                Collider* a = colliders_[i];
                Collider* b = colliders_[j];
                if (!a || !b)
                    continue;

                if (CheckCollisionRecs(a->worldBounds(), b->worldBounds())) {
                    if (a->onCollision_)
                        a->onCollision_(*b);
                    if (b->onCollision_)
                        b->onCollision_(*a);
                }
            }
        }
    }
}
