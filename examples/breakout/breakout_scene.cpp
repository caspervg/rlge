#include "breakout_scene.hpp"

#include "breakout_game.hpp"
#include "circle_collider.hpp"
#include "runtime.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    BreakoutScene::BreakoutScene(Runtime& r) : Scene(r) {}

    void BreakoutScene::enter() {
        camera_ = rlge::Camera();
        setSingleView(camera_);

        // Spawn paddle and ball
        paddle_ = &spawn<Paddle>();
        ball_ = &spawn<Ball>();

        // Spawn bricks
        for (auto row = 0; row < g_cfg.brickRows; ++row) {
            for (auto col = 0; col < g_cfg.brickColumns; ++col) {
                constexpr auto startY = 50.0f;
                constexpr auto startX = 50.0f;
                const float x = startX + col * (g_cfg.brickWidth + g_cfg.brickMargin);
                const float y = startY + row * (g_cfg.brickHeight + g_cfg.brickMargin);
                bricks_.push_back(&spawn<Brick>(x, y));
            }
        }

        // Spawn walls
        topWall_ = &spawn<Wall>(0, -g_cfg.wallThickness, g_cfg.width, g_cfg.wallThickness);
        leftWall_ = &spawn<Wall>(-g_cfg.wallThickness, 0, g_cfg.wallThickness, g_cfg.height);
        rightWall_ = &spawn<Wall>(g_cfg.width, 0, g_cfg.wallThickness, g_cfg.height);

        // Wire up event handlers
        collisionResponses().addHandler([this](Entity& entity, const CollisionEvent& event) { handleCollisionResponse_(entity, event); });
        gameEvents().subscribe<BrickDestroyed>([this](const BrickDestroyed& e) { handleBrickDestroyed_(e); });
        gameEvents().subscribe<BallLost>([this](const BallLost& e) { handleBallLost_(e); });
    }

    void BreakoutScene::handleCollisionResponse_(Entity& entity, const CollisionEvent& event) {
        if (auto* brick = dynamic_cast<Brick*>(&entity)) {
            brick->onCollision(event);
        }
        if (auto* paddle = dynamic_cast<Paddle*>(&entity)) {
            paddle->onCollision(event);
        }
    }

    void BreakoutScene::handleBrickDestroyed_(const BrickDestroyed& e) {
        score_ += e.points;
        camera_.shake(g_cfg.brickHitShakeDuration, g_cfg.brickHitShakeIntensity);
    }

    void BreakoutScene::handleBallLost_(const BallLost& e) {
        lives_--;
        if (lives_ <= 0) {
            gameEvents().enqueue(GameWon{});
        }
        else {
            // resetBall();
        }
    }
} // namespace breakout
