#include "breakout_scene.hpp"

#include <cmath>

#include "breakout_events.hpp"
#include "breakout_level.hpp"
#include "particle_emitter.hpp"
#include "particle_fx.hpp"
#include "powerup.hpp"
#include "runtime.hpp"
#include "safety_net.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    BreakoutScene::BreakoutScene(Runtime& r, BreakoutGame* game) :
        Scene(r), game_(game) {}

    void BreakoutScene::enter() {
        const Level* level = game_->currentLevel();
        numBricksTotal_ = level->bricks.size();
        numBricksLeft_ = numBricksTotal_;
        ballLaunched_ = false;
        canLaunch_ = false;

        camera_ = rlge::Camera();
        setSingleView(camera_);

        // Reset state
        powerUps_.deactivateAll();
        lastBallSpeedMult_ = 1.0f;
        extraBalls_.clear();
        safetyNet_ = nullptr;

        // Setup power-up callbacks for instant effects or scene-side effects
        powerUps_.setCallback([this](const PowerUpType type, const bool activated) {
            if (type == PowerUpType::MultiBall && activated) {
                spawnExtraBalls_(2);
            }
            else if (type == PowerUpType::ExtraLife && activated) {
                game_->gainLife();
            }
            else if (type == PowerUpType::SafetyNet) {
                if (activated && !safetyNet_) {
                    safetyNet_ = &spawn<SafetyNet>(g_cfg.viewPortHeight - 4.0f);
                }
                else if (!activated) {
                    despawnSafetyNet_();
                }
            }
            else if (type == PowerUpType::SlowBall || type == PowerUpType::FastBall) {
                applyBallSpeedMultiplier_();
            }
        });

        // Spawn paddle and ball
        paddle_ = &spawn<Paddle>(*level, powerUps_);
        ball_ = &spawn<Ball>(*level);

        // Spawn bricks
        const auto gridWidth = level->brickColumns * g_cfg.brickWidth + (level->brickColumns - 1) * g_cfg.brickMargin;
        const auto gridHeight = level->brickRows * g_cfg.brickHeight + (level->brickRows - 1) * g_cfg.brickMargin;
        const auto startX = (g_cfg.viewPortWidth - gridWidth) * 0.5f;
        // const auto startY = (g_cfg.viewPortHeight * 0.1f - gridHeight) * 0.5f;
        const auto startY = g_cfg.wallThickness + 20.0f;
        for (const auto& brickConfig : level->bricks) {
            const auto centerScreenX = startX + g_cfg.brickWidth * 0.5f + brickConfig.x * (g_cfg.brickWidth + g_cfg.
                brickMargin);
            const auto centerScreenY = startY + g_cfg.brickHeight * 0.5f + brickConfig.y * (g_cfg.brickHeight + g_cfg.
                brickMargin);
            bricks_.push_back(&spawn<Brick>(brickConfig, centerScreenX, centerScreenY, powerUps_));
        }

        // Spawn walls
        topWall_ = &spawn<Wall>(0, -g_cfg.wallThickness, g_cfg.viewPortWidth, g_cfg.wallThickness);
        leftWall_ = &spawn<Wall>(-g_cfg.wallThickness, 0, g_cfg.wallThickness, g_cfg.viewPortHeight);
        rightWall_ = &spawn<Wall>(g_cfg.viewPortWidth, 0, g_cfg.wallThickness, g_cfg.viewPortHeight);

        // Spawn scoreboard
        scoreBoard_ = &spawn<ScoreBoard>(*game_);
        banner_ = &spawn<Banner>();

        // Wire up event handlers
        collisionResponses().addHandler([this](Entity& entity, const CollisionEvent& event) {
            handleCollisionResponse_(entity, event);
        });
        brickDestroyedHandlerId_ = gameEvents().subscribe<BrickDestroyed>([this](const BrickDestroyed& e) {
            handleBrickDestroyed_(e);
        });
        brickHitHandlerId_ = gameEvents().subscribe<BrickHit>([this](const BrickHit& e) { handleBrickHit_(e); });
        ballLostHandlerId_ = gameEvents().subscribe<BallLost>([this](const BallLost& e) { handleBallLost_(e); });

        // Level start banner and delayed launch enable
        if (banner_) {
            banner_->show("Ready", 1.0f);
        }
        timers().countdown(
            1.0f,
            nullptr,
            [this]() {
                canLaunch_ = true;
            }
        );
    }

    void BreakoutScene::exit() {
        gameEvents().unsubscribe<BrickDestroyed>(brickDestroyedHandlerId_);
        gameEvents().unsubscribe<BrickHit>(brickHitHandlerId_);
        gameEvents().unsubscribe<BallLost>(ballLostHandlerId_);
    }

    void BreakoutScene::update(const float dt) {
        Scene::update(dt);
        powerUps_.update(dt);

        if (!ballLaunched_) {
            auto* ballBody = ball_ ? ball_->get<PhysicsBody>() : nullptr;
            attachBallToPaddle_();
            if (ballBody) {
                ballBody->setVelocity({0.0f, 0.0f}); // Ensure the ball stays parked on the paddle center

                if (canLaunch_ && input().pressed(Action::Fire)) {
                    const Vector2 launchVel = Vector2Scale(game_->currentLevel()->ballVelocityStart,
                                                           powerUps_.ballSpeedMultiplier());
                    ballBody->setVelocity(launchVel);
                    lastBallSpeedMult_ = powerUps_.ballSpeedMultiplier();
                    ballLaunched_ = true;
                }
            }
        }
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
        const Level* level = game_->currentLevel();
        const float scoreMult = powerUps_.scoreMultiplier();

        const auto gridWidth = game_->currentLevel()->brickColumns * g_cfg.brickWidth + (game_->currentLevel()->
            brickColumns - 1) * g_cfg.brickMargin;
        const auto startX = (g_cfg.viewPortWidth - gridWidth) * 0.5f;
        const auto startY = g_cfg.wallThickness + 20.0f;
        const Vector2 brickCenter = {
            .x = startX + g_cfg.brickWidth * 0.5f + e.brick.x * (g_cfg.brickWidth + g_cfg.brickMargin),
            .y = startY + g_cfg.brickHeight * 0.5f + e.brick.y * (g_cfg.brickHeight + g_cfg.brickMargin)
        };

        const auto displayPoints = static_cast<int>(std::lround(e.points * scoreMult));
        const auto brickColor = e.brick.color;

        const auto scoreCfg = BurstEmitterConfig{
            .maxParticles = 3,
            .minLifetime = 0.6f,
            .maxLifetime = 1.0f,
            .minSpeed = 30.0f,
            .maxSpeed = 60.0f,
            .spread = 0.4f,
            .direction = -PI / 2.0f,
            .gravity = {0.0f, 10.0f},
            .startColor = brickColor,
            .endColor = Fade(brickColor, 0.0f)
        };

        spawnBurstEmitter(
            *this,
            brickCenter,
            3,
            scoreCfg,
            [displayPoints](const Particle& p) {
                const auto fontSize = 10 + 8 * (p.life / p.totalLife);
                const auto fontSizeBorder = 10.5f + 8 * (p.life / p.totalLife);
                const auto text = TextFormat("+%d", displayPoints);
                const auto textWidth = MeasureText(text, fontSize);
                const auto textWidthBorder = MeasureText(text, fontSizeBorder);
                DrawTextPro(GetFontDefault(), text, Vector2{p.pos.x - textWidthBorder / 2.0f, p.pos.y}, Vector2{0,0}, p.rotation, fontSizeBorder, 1.0f, WHITE);
                DrawTextPro(GetFontDefault(), text, Vector2{p.pos.x - textWidth / 2.0f, p.pos.y}, Vector2{0,0}, p.rotation, fontSize, 1.0f, p.color);
            }
        );

        camera_.shake(g_cfg.brickHitShakeDuration, g_cfg.brickHitShakeIntensity);

        levelScore_ += displayPoints;

        auto updateSpeed = [level](Ball* b) {
            if (!b)
                return;
            if (auto* body = b->get<PhysicsBody>()) {
                const Vector2 vel = body->velocity();
                if (const auto speed = Vector2Length(vel); speed > 0.0f) {
                    float newSpeed = speed * level->ballVelocityMultiplier;
                    if (level->ballVelocityMaximum > 0.0f && newSpeed > level->ballVelocityMaximum) {
                        newSpeed = level->ballVelocityMaximum;
                    }
                    body->setVelocity(Vector2Scale(Vector2Normalize(vel), newSpeed));
                }
            }
        };

        updateSpeed(ball_);
        for (auto* b : extraBalls_) {
            updateSpeed(b);
        }

        if (numBricksLeft_ > 0 && --numBricksLeft_ == 0) {
            timers().after(0.25, [this] () {
                gameEvents().enqueue(LevelCompleted{levelScore_});
            });
        }
    }

    void BreakoutScene::handleBrickHit_(const BrickHit& e) {
        camera_.shake(g_cfg.brickHitShakeDuration * 0.5f, g_cfg.brickHitShakeIntensity * 0.5f);
    }

    void BreakoutScene::handleBallLost_(const BallLost& e) {
        // Remove references to the lost ball
        if (ball_ && ball_->id() == e.ballId) {
            ball_->destroyDeferred();
            ball_ = nullptr;
        }
        else {
            std::erase_if(extraBalls_, [id = e.ballId](Ball* b) {
                if (b && b->id() == id) {
                    b->destroyDeferred();
                    return true;
                }
                return false;
            });
        }

        // Promote another ball to primary if available
        if (!ball_ && !extraBalls_.empty()) {
            ball_ = extraBalls_.back();
            extraBalls_.pop_back();
        }

        const bool hasBalls = ball_ != nullptr || !extraBalls_.empty();
        if (hasBalls) {
            return;
        }

        // No balls left: lose a life and reset
        powerUps_.deactivateAll();
        clearExtraBalls_();
        despawnSafetyNet_();
        game_->loseLife();
        if (game_->displayLivesRemaining() > 0) {
            ball_ = &spawn<Ball>(*game_->currentLevel());
            ballLaunched_ = false;
            attachBallToPaddle_();
            lastBallSpeedMult_ = powerUps_.ballSpeedMultiplier();

            canLaunch_ = false;
            if (banner_) {
                banner_->show("Get Ready", 0.8f);
            }
            timers().countdown(
                0.8f,
                nullptr,
                [this]() {
                    canLaunch_ = true;
                }
            );
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
        ballTr->position.y = paddleTr->position.y - (g_cfg.paddleHeight / 2.0f) - game_->currentLevel()->ballRadius -
            1.0f;
    }

    void BreakoutScene::spawnExtraBalls_(const int count) {
        if (!ball_)
            return;
        auto* originalTr = ball_->get<rlge::Transform>();
        auto* originalBody = ball_->get<rlge::PhysicsBody>();
        if (!originalTr || !originalBody)
            return;

        const Vector2 baseVel = originalBody->velocity();
        const float speed = Vector2Length(baseVel);

        for (auto i = 0; i < count; i++) {
            Ball& newBall = spawn<Ball>(*game_->currentLevel());
            auto* tr = newBall.get<rlge::Transform>();
            if (tr) {
                tr->position = originalTr->position;
            }

            auto* body = newBall.get<PhysicsBody>();
            if (body) {
                const float angleOffset = (i + 1) * 30.0f * DEG2RAD * (i % 2 == 0 ? 1.0f : -1.0f);
                const float cs = cosf(angleOffset);
                const float sn = sinf(angleOffset);
                Vector2 newVel = {
                    cs * baseVel.x - sn * baseVel.y,
                    sn * baseVel.x + cs * baseVel.y
                };
                if (Vector2Length(newVel) > 0.0f) {
                    newVel = Vector2Scale(Vector2Normalize(newVel), speed);
                }
                body->setVelocity(newVel);
            }

            extraBalls_.push_back(&newBall);
        }
        applyBallSpeedMultiplier_();
    }

    void BreakoutScene::applyBallSpeedMultiplier_() {
        const float newMult = powerUps_.ballSpeedMultiplier();
        if (newMult == lastBallSpeedMult_ || newMult <= 0.0f) {
            return;
        }
        const float ratio = newMult / lastBallSpeedMult_;
        auto scaleBall = [ratio](Ball* b) {
            if (!b)
                return;
            if (auto* body = b->get<rlge::PhysicsBody>()) {
                Vector2 vel = body->velocity();
                vel = Vector2Scale(vel, ratio);
                body->setVelocity(vel);
            }
        };
        scaleBall(ball_);
        for (auto* b : extraBalls_) {
            scaleBall(b);
        }
        lastBallSpeedMult_ = newMult;
    }

    void BreakoutScene::clearExtraBalls_() {
        for (auto* b : extraBalls_) {
            if (b) {
                b->destroyDeferred();
            }
        }
        extraBalls_.clear();
    }

    void BreakoutScene::despawnSafetyNet_() {
        if (safetyNet_) {
            safetyNet_->destroyDeferred();
            safetyNet_ = nullptr;
        }
    }

} // namespace breakout
