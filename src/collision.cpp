#include "collision.hpp"

#include <algorithm>

#include "entity.hpp"
#include "transformer.hpp"

namespace rlge {
    void CollisionSystem::registerCollider(Collider* c) {
        colliders_.push_back(c);
    }

    void CollisionSystem::unregisterCollider(Collider* c) {
        if (updating_) {
            pendingRemovals_.push_back(c);
            return;
        }
        std::erase(colliders_, c);
        compact();
    }

    void CollisionSystem::compact() {
        std::erase(colliders_, nullptr);
    }

    void CollisionSystem::flushPendingRemovals() {
        if (pendingRemovals_.empty())
            return;
        for (auto* c : pendingRemovals_) {
            std::erase(colliders_, c);
        }
        pendingRemovals_.clear();
        compact();
    }

    void CollisionSystem::update(float) {
        flushPendingRemovals();
        updating_ = true;
        const size_t n = colliders_.size();
        for (size_t i = 0; i < n; ++i) {
            Collider* a = colliders_[i];
            if (!a)
                continue;

            for (size_t j = i + 1; j < n; ++j) {
                Collider* b = colliders_[j];
                if (!b)
                    continue;

                if (CheckCollisionRecs(a->worldBounds(), b->worldBounds())) {
                    if (a->onCollision_)
                        a->onCollision_(*b);
                    if (b->onCollision_)
                        b->onCollision_(*a);
                }
            }
        }
        updating_ = false;
        flushPendingRemovals();
    }

    Collider::Collider(Entity& e,
                       CollisionSystem& system,
                       const Rectangle& localBounds,
                       const bool isTrigger)
        : Component(e)
        , system_(system)
        , local_(localBounds)
        , trigger_(isTrigger) {
        system_.registerCollider(this);
    }

    Collider::~Collider() {
        system_.unregisterCollider(this);
    }

    Rectangle Collider::worldBounds() const {
        Rectangle r = local_;
        if (auto* t = entity().get<Transform>()) {
            r.x += t->position.x;
            r.y += t->position.y;
        }
        return r;
    }

    void Collider::setOnCollision(Callback cb) { onCollision_ = std::move(cb); }
    bool Collider::isTrigger() const { return trigger_; }
}
