#pragma once
#include "box_collider.hpp"
#include "breakout_level.hpp"
#include "circle_collider.hpp"
#include "entity.hpp"
#include "physics_body.hpp"
#include "render_entity.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class ScoreBoard final : public RenderEntity {
    public:
        explicit ScoreBoard(Scene& s, const GameState& state);
        void draw() override;

    private:
        const GameState& state_;
    };

    class Wall final : public Entity {
    public:
        explicit Wall(Scene& s, float x, float y, float w, float h);
    };

    class Paddle final : public RenderEntity {
    public:
        explicit Paddle(Scene& s, const Level& level);
        void update(float dt) override;
        void onCollision(const CollisionEvent& event);
        void draw() override;

    private:
        const Level& level_;
        PhysicsBody* physics_{nullptr};
        BoxCollider* coll_{nullptr};
    };

    class Brick final : public RenderEntity {
    public:
        explicit Brick(Scene& s, const BrickConfig& config, float screenX, float screenY);
        void onCollision(const CollisionEvent& event);
        void draw() override;

    private:
        const BrickConfig& config_;
        BoxCollider* coll_{nullptr};
        bool alive_ = true;
        int maxHitPoints_{config_.hitPoints};
        int hitPoints_{config_.hitPoints};
    };

    class Ball final : public RenderEntity {
    public:
        explicit Ball(Scene& s, const Level& level);
        void update(float dt) override;
        void draw() override;

    private:
        const Level& level_;
        PhysicsBody* physics_{nullptr};
        CircleCollider* col_{nullptr};
        bool outOfFrame_ = false;
    };
}