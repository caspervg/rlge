#pragma once
#include <functional>
#include <vector>

#include "component.hpp"
#include "raylib.h"

namespace rlge {
    class Collider;

    class CollisionSystem {
    public:
        void registerCollider(Collider* c);
        void unregisterCollider(Collider* c);
        void update(float);

    private:
        std::vector<Collider*> colliders_;
        bool updating_ = false;
        std::vector<Collider*> pendingRemovals_;

        void compact();
        void flushPendingRemovals();
    };

    class Collider : public Component {
    public:
        using Callback = std::function<void(Collider&)>;

        Collider(Entity& e,
                 CollisionSystem& system,
                 const Rectangle& localBounds,
                 bool isTrigger = true);

        ~Collider() override;

        Rectangle worldBounds() const;

        void setOnCollision(Callback cb);
        bool isTrigger() const;

    private:
        friend class CollisionSystem;

        CollisionSystem& system_;
        Rectangle local_;
        bool trigger_;
        Callback onCollision_;
    };
}
