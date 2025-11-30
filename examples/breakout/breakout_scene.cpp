#include "breakout_scene.hpp"

#include "breakout_events.hpp"
#include "breakout_level.hpp"
#include "circle_collider.hpp"
#include "runtime.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    BreakoutScene::BreakoutScene(Runtime& r, const Level* level) : Scene(r), level_(*level) {}

    void BreakoutScene::enter() {
        camera_ = rlge::Camera();
        setSingleView(camera_);

        // Spawn paddle and ball
        paddle_ = &spawn<Paddle>(level_);
        ball_ = &spawn<Ball>(level_);

        // Spawn bricks
        const auto gridWidth = level_.brickColumns * g_cfg.brickWidth + (level_.brickColumns - 1) * g_cfg.brickMargin;
        const auto gridHeight = level_.brickRows * g_cfg.brickHeight + (level_.brickRows - 1) * g_cfg.brickMargin;
        const auto startX = (g_cfg.viewPortWidth - gridWidth) * 0.5f;
        const auto startY = (g_cfg.viewPortHeight * 0.1f - gridHeight) * 0.5f;
        for (const auto& brickConfig : level_.bricks) {
            const auto centerScreenX = startX + g_cfg.brickWidth * 0.5f + brickConfig.x * (g_cfg.brickWidth + g_cfg.brickMargin);
            const auto centerScreenY = startY + g_cfg.brickHeight * 0.5f + brickConfig.y * (g_cfg.brickHeight + g_cfg.brickMargin);
            bricks_.push_back(&spawn<Brick>(brickConfig, centerScreenX, centerScreenY));
        }

        state_ = {
            .levelName = level_.name,
            .level = 0,
            .numLevels = 3,
            .score = 0,
            .lives = level_.lives,
            .numBricksLeft = bricks_.size(),
            .numBricksTotal = bricks_.size(),
        };

        // Spawn walls
        topWall_ = &spawn<Wall>(0, -g_cfg.wallThickness, g_cfg.viewPortWidth, g_cfg.wallThickness);
        leftWall_ = &spawn<Wall>(-g_cfg.wallThickness, 0, g_cfg.wallThickness, g_cfg.viewPortHeight);
        rightWall_ = &spawn<Wall>(g_cfg.viewPortWidth, 0, g_cfg.wallThickness, g_cfg.viewPortHeight);

        // Spawn scoreboard
        scoreBoard_ = &spawn<ScoreBoard>(state_);

        // Wire up event handlers
        collisionResponses().addHandler([this](Entity& entity, const CollisionEvent& event) { handleCollisionResponse_(entity, event); });
        brickDestroyedHandlerId_ = gameEvents().subscribe<BrickDestroyed>([this](const BrickDestroyed& e) { handleBrickDestroyed_(e); });
        brickHitHandlerId_ = gameEvents().subscribe<BrickHit>([this](const BrickHit& e) { handleBrickHit_(e); });
        ballLostHandlerId_ = gameEvents().subscribe<BallLost>([this](const BallLost& e) { handleBallLost_(e); });
    }

    void BreakoutScene::exit() {
        gameEvents().unsubscribe<BrickDestroyed>(brickDestroyedHandlerId_);
        gameEvents().unsubscribe<BrickHit>(brickHitHandlerId_);
        gameEvents().unsubscribe<BallLost>(ballLostHandlerId_);
    }

    void BreakoutScene::update(const float dt) {
        Scene::update(dt);

        if (!ballLaunched_) {
            auto* ballBody = ball_ ? ball_->get<PhysicsBody>() : nullptr;
            attachBallToPaddle_();
            if (ballBody) {
                ballBody->setVelocity({0.0f, 0.0f}); // Ensure the ball stays parked on the paddle center

                if (input().pressed(Action::Fire)) {
                    ballBody->setVelocity(level_.ballVelocityStart);
                    ballLaunched_ = true;
                }
            }
        }
    }

    const GameState& BreakoutScene::gameState() const {
        return state_;
    }

    void BreakoutScene::resetBall_() {
        ball_ = &spawn<Ball>(level_);
        ballLaunched_ = false;
        attachBallToPaddle_();
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
        state_.score += e.points;
        camera_.shake(g_cfg.brickHitShakeDuration, g_cfg.brickHitShakeIntensity);

        if (auto* body = ball_ ? ball_->get<PhysicsBody>() : nullptr) {
            // Increase ball speed
            const Vector2 vel = body->velocity();
            if (const auto speed = Vector2Length(vel); speed > 0.0f) {
                float newSpeed = speed * level_.ballVelocityMultiplier;
                if (level_.ballVelocityMaximum > 0.0f && newSpeed > level_.ballVelocityMaximum) {
                    newSpeed = level_.ballVelocityMaximum;
                }
                body->setVelocity(Vector2Scale(Vector2Normalize(vel), newSpeed));
            }
        }

        if (state_.numBricksLeft > 0 && --state_.numBricksLeft == 0) {
            gameEvents().enqueue(LevelCompleted{state_.score});
        }
    }

    void BreakoutScene::handleBrickHit_(const BrickHit& e) {
        camera_.shake(g_cfg.brickHitShakeDuration * 0.5, g_cfg.brickHitShakeIntensity * 0.5);
    }

    void BreakoutScene::handleBallLost_(const BallLost& e) {
        state_.lives--;
        if (state_.lives <= 0) {
            gameEvents().enqueue(GameLost{state_.score});
        }
        else {
            resetBall_();
        }
    }

    void BreakoutScene::attachBallToPaddle_() {
        if (!ball_ || !paddle_)
            return;

        const auto* paddleTr = paddle_->get<rlge::Transform>();
        auto* ballTr = ball_->get<rlge::Transform>();
        if (!paddleTr || !ballTr)
            return;

        ballTr->position.x = paddleTr->position.x;
        ballTr->position.y = paddleTr->position.y - (g_cfg.paddleHeight / 2.0f) - level_.ballRadius - 1.0f;
    }

} // namespace breakout
