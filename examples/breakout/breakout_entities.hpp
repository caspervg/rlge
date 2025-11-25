#pragma once
#include "box_collider.hpp"
#include "circle_collider.hpp"
#include "entity.hpp"
#include "physics_body.hpp"
#include "render_entity.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class Wall final : public Entity {
    public:
        explicit Wall(Scene& s, float x, float y, float w, float h);
    };

    class Paddle final : public RenderEntity {
    public:
        explicit Paddle(Scene& s);
        void update(float dt) override;
        void onCollision(const CollisionEvent& event);
        void draw() override;

    private:
        PhysicsBody* physics_{nullptr};
        BoxCollider* coll_{nullptr};
    };

    class Brick final : public RenderEntity {
    public:
        explicit Brick(Scene& s, float x, float y);
        void onCollision(const CollisionEvent& event);
        void draw() override;

    private:
        BoxCollider* coll_{nullptr};
        bool alive_ = true;
    };

    class Ball final : public RenderEntity {
    public:
        explicit Ball(Scene& s);
        void update(float dt) override;
        void draw() override;

    private:
        PhysicsBody* physics_{nullptr};
        CircleCollider* col_{nullptr};
        bool outOfFrame_ = false;
    };
}