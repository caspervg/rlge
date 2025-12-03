#include "breakout_entities.hpp"

#include <format>

#include "breakout_config.hpp"
#include "breakout_events.hpp"
#include "breakout_game.hpp"
#include "powerup_entity.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    ScoreBoard::ScoreBoard(Scene& s, const BreakoutGame& game) : RenderEntity(s), game_(game) {}

    void ScoreBoard::draw() {
        rq().submitUI([this] {
            const auto line1 = std::format("L: {}/{}", game_.displayLevelNumber(), game_.numLevels());
            const auto line2 = std::format("S: {}", game_.displayTotalScore());
            const auto line3 = std::format("H: {}", game_.displayLivesRemaining());
            DrawText(line1.c_str(), 10, 10, 20, WHITE);
            DrawText(line2.c_str(), 10, 30, 20, GREEN);
            DrawText(line3.c_str(), 10, 50, 20, RED);
        });
    }

    Wall::Wall(Scene& s, const float x, const float y, const float w, const float h) :
        Entity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x + w / 2.0f, y + h / 2.0f};

        add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_WORLD,
                         ColliderLayerMask::LAYER_BULLET, Rectangle{-w / 2.0f, -h / 2.0f, w, h}, false);
    }

    Paddle::Paddle(Scene& s, const Level& level, PowerUpManager& powerUps) :
        RenderEntity(s), level_(level), powerUps_(powerUps) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.viewPortWidth / 2.0f, g_cfg.viewPortHeight - 20.0f - g_cfg.paddleHeight / 2.0f};

        PhysicsBodyConfig conf = {.mass = 1.0f, .velocity = {0.f, 0.f}, .type = BodyType::Kinematic};
        physics_ = &add<PhysicsBody>(conf);

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_PLAYER,
                                  toLayerMask(static_cast<uint32_t>(ColliderLayerMask::LAYER_BULLET) | static_cast<uint32_t>(ColliderLayerMask::LAYER_ITEM)),
                                  Rectangle{-level_.paddleWidth / 2.0f, -g_cfg.paddleHeight / 2.0f,
                                            level_.paddleWidth * 1.0f, g_cfg.paddleHeight * 1.0f},
                                  false);
    }

    void Paddle::update(const float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        const float widthMult = powerUps_.paddleWidthMultiplier();
        const float effectiveWidth = level_.paddleWidth * widthMult;

        if (input().down(Action::MoveLeft)) {
            tr->position.x -= level_.paddleSpeed * dt;
        }
        if (input().down(Action::MoveRight)) {
            tr->position.x += level_.paddleSpeed * dt;
        }

        tr->position.x += input().axisValue(Action::MoveLeft) * level_.paddleSpeed * dt;
        tr->position.x -= input().axisValue(Action::MoveRight) * level_.paddleSpeed * dt;


        const float halfWidth = effectiveWidth / 2.0f;
        if (tr->position.x < halfWidth) {
            tr->position.x = halfWidth;
        }
        if (tr->position.x > g_cfg.viewPortWidth - halfWidth) {
            tr->position.x = g_cfg.viewPortWidth - halfWidth;
        }

        // Keep collider in sync with current width
        if (coll_) {
            coll_->setLocalBounds(Rectangle{
                -effectiveWidth / 2.0f,
                -g_cfg.paddleHeight / 2.0f,
                effectiveWidth,
                static_cast<float>(g_cfg.paddleHeight)
            });
        }
    }

    void Paddle::onCollision(const CollisionEvent& event) {
        // Pick up power-ups
        if (auto* powerUp = dynamic_cast<PowerUp*>(&event.colliderB->entity())) {
            if (!powerUp->isCollected()) {
                powerUp->collect();
                powerUps_.activate(powerUp->type());
                return;
            }
        }

        if (auto* ballPhysics = event.colliderB->entity().get<PhysicsBody>()) {
            const auto* tr = get<rlge::Transform>();
            const auto* ballTr = event.colliderB->entity().get<rlge::Transform>();

            if (!tr || !ballTr)
                return;

            // Calculate hit position (-1.0 to 1.0)
            const auto hitOffset = (ballTr->position.x - tr->position.x) / ((level_.paddleWidth * powerUps_.paddleWidthMultiplier()) / 2.0f);

            // Modify the ball angle based on hit position
            const auto angle = hitOffset * g_cfg.maxBallPaddleDeflectionAngle * DEG2RAD;
            const auto speed = Vector2Length(ballPhysics->velocity());

            const auto newVel = Vector2{
                sinf(angle) * speed,
                -fabsf(cosf(angle) * speed) // Force negative (upward)
            };
            ballPhysics->setVelocity(newVel);
        }
    }


    void Paddle::draw() {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<rlge::Transform>();
            if (!tr)
                return;
            const float effectiveWidth = level_.paddleWidth * powerUps_.paddleWidthMultiplier();
            DrawRectangle(static_cast<int>(tr->position.x - effectiveWidth / 2.0f),
                          static_cast<int>(tr->position.y - g_cfg.paddleHeight / 2.0f), static_cast<int>(effectiveWidth),
                          g_cfg.paddleHeight, g_cfg.paddleColor);
        });
    }

    Brick::Brick(Scene& s, const BrickConfig& config, const float screenX, const float screenY, PowerUpManager& powerUps) :
        RenderEntity(s), config_(config), powerUps_(powerUps) {
        auto& tr = add<rlge::Transform>();
        tr.position = {screenX, screenY};

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_WORLD,
                                  ColliderLayerMask::LAYER_BULLET,
                                  Rectangle{-g_cfg.brickWidth / 2.0f, -g_cfg.brickHeight / 2.0f,
                                            g_cfg.brickWidth * 1.0f, g_cfg.brickHeight * 1.0f},
                                  false);
    }

    void Brick::onCollision(const CollisionEvent& event) {
        if (event.state != CollisionState::Enter || !alive_)
            return;

        if (--hitPoints_ <= 0) {
            alive_ = false;
            coll_->unregisterCollider();
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
            {PowerUpType::FastBall, 3},   // Negative, less common
            {PowerUpType::NarrowPaddle, 2},  // Negative, rare
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

    Ball::Ball(Scene& s, const Level& level) :
        RenderEntity(s), level_(level) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.viewPortWidth / 2.0f, g_cfg.viewPortHeight / 2.0f};

        PhysicsBodyConfig conf = {
            .mass = 1.0f, .velocity = level_.ballVelocityStart, .gravity = {0.0f, 0.0f}, .type = BodyType::Dynamic};
        physics_ = &add<PhysicsBody>(conf);

        col_ = &add<CircleCollider>(scene().collisions(), ColliderType::Solid, CLM::LAYER_BULLET,
                                    toLayerMask(CLM::LAYER_PLAYER | CLM::LAYER_WORLD), Vector2{0.0f, 0.0f},
                                    level_.ballRadius, false);
    }

    void Ball::update(const float dt) {
        RenderEntity::update(dt);
        const auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        // The ball fell off
        if (tr->position.y > g_cfg.viewPortHeight && !outOfFrame_) {
            outOfFrame_ = true;
            // Defer handling so the scene isn't mutated mid-update.
            scene().gameEvents().enqueue(BallLost{ id() });
        }
    }

    void Ball::draw() {
        RenderEntity::draw();
        rq().submitWorld([this] {
            const auto* tr = get<rlge::Transform>();
            if (!tr)
                return;
            DrawCircleV(tr->position, level_.ballRadius, g_cfg.ballColor);
        });
    }


} // namespace breakout
