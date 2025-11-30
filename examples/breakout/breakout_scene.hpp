#pragma once
#include "breakout_config.hpp"
#include "breakout_entities.hpp"
#include "breakout_events.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class BreakoutScene final : public Scene {
    public:
        explicit BreakoutScene(Runtime& r, const Level* level);

        void enter() override;
        void exit() override;
        void update(float dt) override;
        [[nodiscard]] const GameState& gameState() const;

    private:
        void resetBall_();
        void handleCollisionResponse_(Entity& entity, const CollisionEvent& event);
        void handleBrickDestroyed_(const BrickDestroyed& e);
        void handleBrickHit_(const BrickHit& e);
        void handleBallLost_(const BallLost& e);
        void attachBallToPaddle_();

    private:
        const Level& level_;
        rlge::Camera camera_;
        GameState state_;
        bool ballLaunched_{false};
        Paddle* paddle_{nullptr};
        Ball* ball_{nullptr};
        Wall* leftWall_{nullptr};
        Wall* rightWall_{nullptr};
        Wall* topWall_{nullptr};
        ScoreBoard* scoreBoard_{nullptr};
        std::vector<Brick*> bricks_{};
        EventBus::SubscriptionId collisionHandlerId_{0};
        EventBus::SubscriptionId ballLostHandlerId_{0};
        EventBus::SubscriptionId brickDestroyedHandlerId_{0};
        EventBus::SubscriptionId brickHitHandlerId_{0};
    };
}