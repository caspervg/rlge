#pragma once

#include <memory>

#include "camera.hpp"
#include "scene.hpp"
#include "snake_game.hpp"

namespace rlge {
    class BurstParticleEmitter;
    class SpriteSheet;
}

namespace snake {

    class Background;
    class SnakeHead;
    class SnakeBody;
    class BorderTiles;
    class AppleSprite;
    class FpsCounter;
    class Scoreboard;

    class GameScene final : public rlge::Scene {
    public:
        explicit GameScene(rlge::Runtime& r);
        ~GameScene() override;

        void enter() override;
        void update(float dt) override;
        void exit() override;

    private:
        Game game_;

        Background* bg_{nullptr};
        SnakeHead* snake_{nullptr};
        SnakeBody* snakeBody_{nullptr};
        BorderTiles* borders_{nullptr};
        AppleSprite* apple_{nullptr};
        FpsCounter* fps_{nullptr};
        Scoreboard* scoreboard_{nullptr};
        rlge::BurstParticleEmitter* deathFx_{nullptr};

        rlge::Camera2DController camera_;

        std::unique_ptr<rlge::SpriteSheet> spriteSheet_;
        rlge::EventBus::SubscriptionId appleSubId_{0};
        rlge::EventBus::SubscriptionId diedSubId_{0};
        int score_ = 0;
        bool deathPending_{false};
        float deathTimer_{0.0f};
    };

} // namespace snake
