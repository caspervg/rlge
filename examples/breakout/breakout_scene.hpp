#pragma once
#include "breakout_config.hpp"
#include "breakout_entities.hpp"
#include "breakout_events.hpp"
#include "breakout_game.hpp"
#include "powerup_manager.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class SafetyNet;

    class BreakoutScene final : public Scene {
    public:
        explicit BreakoutScene(Runtime& r, BreakoutGame* game);

        void enter() override;
        void exit() override;
        void update(float dt) override;

    private:
        void handleCollisionResponse_(Entity& entity, const CollisionEvent& event);
        void handleBrickDestroyed_(const BrickDestroyed& e);
        void handleBrickHit_(const BrickHit& e);
        void handleBallLost_(const BallLost& e);
        void attachBallToPaddle_();
        void spawnExtraBalls_(int count);
        void applyBallSpeedMultiplier_();
        void clearExtraBalls_();
        void despawnSafetyNet_();

    private:
        BreakoutGame* game_{nullptr};
        rlge::Camera camera_;
        bool ballLaunched_{false};
        int levelScore_{0};
        unsigned int numBricksLeft_{0};
        unsigned int numBricksTotal_{0};
        Paddle* paddle_{nullptr};
        Ball* ball_{nullptr};
        std::vector<Ball*> extraBalls_;
        Wall* leftWall_{nullptr};
        Wall* rightWall_{nullptr};
        Wall* topWall_{nullptr};
        ScoreBoard* scoreBoard_{nullptr};
        std::vector<Brick*> bricks_{};
        PowerUpManager powerUps_;
        SafetyNet* safetyNet_{nullptr};
        float lastBallSpeedMult_{1.0f};
        EventBus::SubscriptionId collisionHandlerId_{0};
        EventBus::SubscriptionId ballLostHandlerId_{0};
        EventBus::SubscriptionId brickDestroyedHandlerId_{0};
        EventBus::SubscriptionId brickHitHandlerId_{0};
    };
}
