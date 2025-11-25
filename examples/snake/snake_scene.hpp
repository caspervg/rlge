#pragma once

#include <memory>

#include "camera.hpp"
#include "debug.hpp"
#include "scene.hpp"
#include "snake_game.hpp"

namespace rlge {
    class BurstParticleEmitter;
    class SpriteSheet;
}

namespace snake {

    class Background;
    class Scoreboard;
    class SnakeHead;
    class SnakeBody;
    class BorderTiles;
    class AppleSprite;
    class FpsCounter;

    class GameScene final : public rlge::Scene, public rlge::HasDebugOverlay {
    public:
        explicit GameScene(rlge::Runtime& r);
        ~GameScene() override;

        void enter() override;
        void update(float dt) override;
        void exit() override;
        void debugOverlay() override;

    private:
        Game game_;

        Background* bg_{nullptr};
        Scoreboard* scoreboard_{nullptr};
        SnakeHead* snake_{nullptr};
        SnakeBody* snakeBody_{nullptr};
        BorderTiles* borders_{nullptr};
        AppleSprite* apple_{nullptr};
        FpsCounter* fps_{nullptr};
        rlge::BurstParticleEmitter* deathFx_{nullptr};

        rlge::Camera camera_;

        std::unique_ptr<rlge::SpriteSheet> spriteSheet_;
        rlge::EventBus::SubscriptionId appleSubId_{0};
        rlge::EventBus::SubscriptionId diedSubId_{0};
        int score_ = 0;
        bool deathPending_{false};
        float deathTimer_{0.0f};
    };

} // namespace snake
