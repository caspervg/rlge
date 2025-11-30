#include "breakout_entities.hpp"
#include "breakout_config.hpp"
#include "breakout_events.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    ScoreBoard::ScoreBoard(Scene& s, const GameState& state) : RenderEntity(s), state_(state) {}

    void ScoreBoard::draw() {
        rq().submitUI([this] {
            const auto line1 = std::format("L: {}/{}", state_.level + 1, state_.numLevels);
            const auto line2 = std::format("S: {}", state_.score);
            const auto line3 = std::format("H: {}", state_.lives);
            const auto line4 = std::format("B: {}/{}", state_.numBricksLeft, state_.numBricksTotal);
            DrawText(line1.c_str(), 10, 10, 20, WHITE);
            DrawText(line2.c_str(), 10, 30, 20, GREEN);
            DrawText(line3.c_str(), 10, 50, 20, RED);
            DrawText(line4.c_str(), 10, 70, 20, WHITE);
        });
    }

    Wall::Wall(Scene& s, const float x, const float y, const float w, const float h) :
        Entity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x + w / 2.0f, y + h / 2.0f};

        add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_WORLD,
                         ColliderLayerMask::LAYER_BULLET, Rectangle{-w / 2.0f, -h / 2.0f, w, h}, false);
    }

    Paddle::Paddle(Scene& s, const Level& level) :
        RenderEntity(s), level_(level) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.viewPortWidth / 2.0f, g_cfg.viewPortHeight - 20.0f - g_cfg.paddleHeight / 2.0f};

        PhysicsBodyConfig conf = {.mass = 1.0f, .velocity = {0.f, 0.f}, .type = BodyType::Kinematic};
        physics_ = &add<PhysicsBody>(conf);

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_PLAYER,
                                  ColliderLayerMask::LAYER_BULLET,
                                  Rectangle{-level_.paddleWidth / 2.0f, -g_cfg.paddleHeight / 2.0f,
                                            level_.paddleWidth * 1.0f, g_cfg.paddleHeight * 1.0f},
                                  false);
    }

    void Paddle::update(const float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        if (input().down(Action::MoveLeft)) {
            tr->position.x -= level_.paddleSpeed * dt;
        }
        if (input().down(Action::MoveRight)) {
            tr->position.x += level_.paddleSpeed * dt;
        }

        tr->position.x += input().axisValue(Action::MoveLeft) * level_.paddleSpeed * dt;
        tr->position.x -= input().axisValue(Action::MoveRight) * level_.paddleSpeed * dt;


        const float halfWidth = level_.paddleWidth / 2.0f;
        if (tr->position.x < halfWidth) {
            tr->position.x = halfWidth;
        }
        if (tr->position.x > g_cfg.viewPortWidth - halfWidth) {
            tr->position.x = g_cfg.viewPortWidth - halfWidth;
        }
    }

    void Paddle::onCollision(const CollisionEvent& event) {
        if (auto* ballPhysics = event.colliderB->entity().get<PhysicsBody>()) {
            const auto* tr = get<rlge::Transform>();
            const auto* ballTr = event.colliderB->entity().get<rlge::Transform>();

            if (!tr || !ballTr)
                return;

            // Calculate hit position (-1.0 to 1.0)
            const auto hitOffset = (ballTr->position.x - tr->position.x) / (g_cfg.paddleWidth / 2.0f);

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
            DrawRectangle(static_cast<int>(tr->position.x - level_.paddleWidth / 2.0f),
                          static_cast<int>(tr->position.y - g_cfg.paddleHeight / 2.0f), level_.paddleWidth,
                          g_cfg.paddleHeight, g_cfg.paddleColor);
        });
    }

    Brick::Brick(Scene& s, const BrickConfig& config, const float screenX, const float screenY) :
        RenderEntity(s), config_(config) {
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
            scene().gameEvents().enqueue(BrickDestroyed{config_.hitPoints * 10, config_});
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

    void Ball::update(float dt) {
        RenderEntity::update(dt);
        const auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        // The ball fell off
        if (tr->position.y > g_cfg.viewPortHeight && !outOfFrame_) {
            outOfFrame_ = true;
            scene().gameEvents().publish(BallLost{});
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
