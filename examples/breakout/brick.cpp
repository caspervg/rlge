#include "brick.hpp"

#include "breakout_config.hpp"
#include "breakout_events.hpp"
#include "powerup.hpp"
#include "raylib.h"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Brick::Brick(Scene& s, const BrickConfig& config, const float screenX, const float screenY, PowerUpManager& powerUps) :
        RenderEntity(s), config_(config), powerUps_(powerUps) {
        auto& tr = add<rlge::Transform>();
        tr.position = {screenX, screenY};

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_kinematicBody,
            .gravityScale = 0.0f,
            .fixedRotation = true
        };
        body_ = &add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.0f,
            .restitution = 1.0f,
            .isSensor = false,
            .layer = CLM::LAYER_WORLD,
            .mask = CLM::LAYER_BULLET
        };
        body_->addBoxFixture(g_cfg.brickWidth, g_cfg.brickHeight, fixtureCfg);

        body_->setOnCollisionEnter([this](const CollisionEvent& event) {
            onCollision(event);
        });
    }

    void Brick::onCollision(const CollisionEvent& event) {
        if (event.state != CollisionState::Enter || !alive_)
            return;

        if (--hitPoints_ <= 0) {
            alive_ = false;
            if (body_) {
                // Destroy the body to remove from physics simulation
                body_->body()->SetEnabled(false);
            }
            spawnPowerUpsIfApplicable();
            scene().gameEvents().enqueue(BrickDestroyed{config_.hitPoints * 10, config_});
            destroyDeferred();
        } else {
            scene().gameEvents().enqueue(BrickHit{config_});
        }
    }

    void Brick::draw() {
        RenderEntity::draw();
        if (!alive_)
            return;

        rq().submitWorld([this] {
            const auto* tr = get<rlge::Transform>();
            if (!tr)
                return;
            Color brickColor = config_.color;
            brickColor.a = static_cast<unsigned char>(
                (static_cast<float>(hitPoints_) / static_cast<float>(maxHitPoints_)) * brickColor.a);

            DrawRectangle(static_cast<int>(tr->position.x - g_cfg.brickWidth / 2.0f),
                          static_cast<int>(tr->position.y - g_cfg.brickHeight / 2.0f), g_cfg.brickWidth,
                          g_cfg.brickHeight, brickColor);
        });
    }

    void Brick::spawnPowerUpsIfApplicable() {
        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        if (!config_.containedPowerUps.empty()) {
            for (auto type : config_.containedPowerUps) {
                scene().spawn<PowerUp>(type, tr->position.x, tr->position.y);
            }
            return;
        }

        if (config_.powerUpDropChance > 0.0f) {
            const float roll = static_cast<float>(GetRandomValue(0, 1000)) / 1000.0f;
            if (roll <= config_.powerUpDropChance) {
                const auto type = getRandomPowerUpType();
                scene().spawn<PowerUp>(type, tr->position.x, tr->position.y);
            }
        }
    }

    PowerUpType Brick::getRandomPowerUpType() {
        static const std::vector<std::pair<PowerUpType, int>> weights = {
            {PowerUpType::WidePaddle, 20},
            {PowerUpType::MultiBall, 15},
            {PowerUpType::SlowBall, 15},
            {PowerUpType::ExtraLife, 5},
            {PowerUpType::FireBall, 10},
            {PowerUpType::LaserPaddle, 10},
            {PowerUpType::ScoreMultiplier, 10},
            {PowerUpType::SafetyNet, 10},
            {PowerUpType::FastBall, 3},
            {PowerUpType::NarrowPaddle, 2},
        };

        int total = 0;
        for (const auto& [type, weight] : weights) total += weight;

        int roll = GetRandomValue(0, total - 1);
        for (const auto& [type, weight] : weights) {
            roll -= weight;
            if (roll < 0) return type;
        }
        return PowerUpType::WidePaddle;
    }
} // namespace breakout
