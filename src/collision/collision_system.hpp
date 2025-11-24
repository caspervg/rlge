#pragma once
#include <algorithm>
#include <set>
#include <vector>

#include "collider_types.hpp"
#include "debug.hpp"

namespace rlge {
    class Entity;
    class Collider;
    class Scene;
    struct CollisionManifold;
    struct CollisionEvent;

    class CollisionSystem : public HasDebugOverlay {
    private:
        struct CollisionPair {
            Collider* a;
            Collider* b;

            bool operator==(const CollisionPair& other) const { return a == other.a && b == other.b; } // || a == other.b && b == other.a; }

            bool operator<(const CollisionPair& other) const {
                // Normalize so (A,B) == (B,A)
                auto [minA, maxA] = std::minmax(a, b);
                auto [minB, maxB] = std::minmax(other.a, other.b);
                if (minA != minB) return minA < minB;
                return maxA < maxB;
            }
        };

    public:
        void registerCollider(Collider* c);
        void unregisterCollider(Collider* c);
        void update(float dt);
        [[nodiscard]] const std::vector<CollisionEvent>& collisionEvents() const;
        void setDebug(bool debug);
        [[nodiscard]] bool debug() const;
        void debugOverlay() override;

    private:
        void compact_();
        void flushPendingRemovals_();
        void resolve_(Collider* a, Collider* b, const CollisionManifold& manifold);

    private:
        bool updating_ = false;
        std::vector<Collider*> colliders_;
        std::vector<CollisionEvent> collisionEvents_;
        std::set<CollisionPair> collisionPairsThisFrame_;
        std::set<CollisionPair> collisionPairsLastFrame_;
        std::vector<Collider*> pendingRemovals_;
        bool debug_ = false;
    };

    class CollisionResponseSystem {
    public:
        using CollisionResponseHandler = std::function<void(Entity&, const CollisionEvent&)>;
        void addHandler(CollisionResponseHandler handler);
        void update(Scene& scene);

    private:
        void processEntity_(Entity& entity, const CollisionEvent& event) const;
        void resolve_(Collider* a, Collider* b, const CollisionManifold& manifold);

    private:
        std::vector<CollisionResponseHandler> handlers_;
    };
}
