#pragma once
#include <vector>

#include "debug.hpp"

namespace rlge {
    class Collider;
    struct CollisionManifold;

    class CollisionSystem : public HasDebugOverlay {
    public:
        void registerCollider(Collider* c);
        void unregisterCollider(Collider* c);
        void update(float dt);
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
        std::vector<Collider*> pendingRemovals_;
        bool debug_ = false;
    };
}
