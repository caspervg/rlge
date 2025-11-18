#include "collision.hpp"

#include "entity.hpp"
#include "transformer.hpp"

namespace rlge {
    void CollisionSystem::registerCollider(Collider* c) {
        colliders_.push_back(c);
    }

    void CollisionSystem::unregisterCollider(Collider* c) {
        for (size_t i = 0; i < colliders_.size(); ++i) {
            if (colliders_[i] == c) {
                colliders_[i] = colliders_.back();
                colliders_.pop_back();
                return;
            }
        }
    }

    void CollisionSystem::update(float) const {
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

