#pragma once
#include "breakout_config.hpp"
#include "breakout_entities.hpp"
#include "breakout_game.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class BreakoutScene final : public Scene {
    public:
        explicit BreakoutScene(Runtime& r);

        void enter() override;
        void exit() override;

    private:
        void resetBall_();
        void handleCollisionResponse_(Entity& entity, const CollisionEvent& event);
        void handleBrickDestroyed_(const BrickDestroyed& e);
        void handleBallLost_(const BallLost& e);

    private:
        rlge::Camera camera_;
        int lives_ = g_cfg.initialLives;
        int score_ = 0;
        Paddle* paddle_{nullptr};
        Ball* ball_{nullptr};
        Wall* leftWall_{nullptr};
        Wall* rightWall_{nullptr};
        Wall* topWall_{nullptr};
        std::vector<Brick*> bricks_;
        EventBus::SubscriptionId collisionHandlerId_{0};
        EventBus::SubscriptionId ballLostHandlerId_{0};
        EventBus::SubscriptionId brickDestroyedHandlerId_{0};

    };
}