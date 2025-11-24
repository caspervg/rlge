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

            // Constructor ensures canonical ordering: a <= b
            CollisionPair(Collider* first, Collider* second) {
                auto [min, max] = std::minmax(first, second);
                a = min;
                b = max;
            }

            bool operator==(const CollisionPair& other) const {
                // With canonical ordering, simple equality check is sufficient
                return a == other.a && b == other.b;
            }

            bool operator<(const CollisionPair& other) const {
                // With canonical ordering, simple comparison is sufficient
                if (a != other.a) return a < other.a;
                return b < other.b;
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
