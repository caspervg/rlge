#include "breakout_entities.hpp"
#include "breakout_config.hpp"
#include "breakout_game.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Wall::Wall(Scene& s, const float x, const float y, const float w, const float h) : Entity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x + w / 2.0f, y + h / 2.0f};

        add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_WORLD,
                         ColliderLayerMask::LAYER_BULLET, Rectangle{-w / 2.0f, -h / 2.0f, w, h}, false);
    }

    Paddle::Paddle(Scene& s) : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.width / 2.0f, g_cfg.height - 20.0f - g_cfg.paddleHeight / 2.0f};

        PhysicsBodyConfig conf = {.mass = 1.0f, .velocity = {0.f, 0.f}, .type = BodyType::Kinematic};
        physics_ = &add<PhysicsBody>(conf);

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, ColliderLayerMask::LAYER_PLAYER,
                                  ColliderLayerMask::LAYER_BULLET,
                                  Rectangle{-g_cfg.paddleWidth / 2.0f, -g_cfg.paddleHeight / 2.0f,
                                            g_cfg.paddleWidth * 1.0f, g_cfg.paddleHeight * 1.0f},
                                  false);
    }

    void Paddle::update(const float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        if (input().down(Action::MoveLeft)) {
            tr->position.x -= g_cfg.paddleSpeed * dt;
        }
        if (input().down(Action::MoveRight)) {
            tr->position.x += g_cfg.paddleSpeed * dt;
        }

        tr->position.x += input().axisValue(Action::MoveLeft) * g_cfg.paddleSpeed * dt;
        tr->position.x -= input().axisValue(Action::MoveRight) * g_cfg.paddleSpeed * dt;


        const float halfWidth = g_cfg.paddleWidth / 2.0f;
        if (tr->position.x < halfWidth) {
            tr->position.x = halfWidth;
        }
        if (tr->position.x > g_cfg.width - halfWidth) {
            tr->position.x = g_cfg.width - halfWidth;
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

            // Modify ball angle based on hit position
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
            DrawRectangle(static_cast<int>(tr->position.x - g_cfg.paddleWidth / 2.0f),
                          static_cast<int>(tr->position.y - g_cfg.paddleHeight / 2.0f), g_cfg.paddleWidth,
                          g_cfg.paddleHeight, g_cfg.paddleColor);
        });
    }

    Brick::Brick(Scene& s, const float x, const float y) : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x, y};

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Solid, ColliderLayerMask::LAYER_WORLD,
                                  ColliderLayerMask::LAYER_BULLET,
                                  Rectangle{-g_cfg.brickWidth / 2.0f, -g_cfg.brickHeight / 2.0f,
                                            g_cfg.brickWidth * 1.0f, g_cfg.brickHeight * 1.0f},
                                  false);
    }

    void Brick::onCollision(const CollisionEvent& event) {
        coll_->unregisterCollider(); // Remove collider to prevent further collisions
        if (event.state == CollisionState::Enter && alive_) {
            alive_ = false;
            scene().gameEvents().enqueue(BrickDestroyed{10});
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
            DrawRectangle(static_cast<int>(tr->position.x - g_cfg.brickWidth / 2.0f),
                          static_cast<int>(tr->position.y - g_cfg.brickHeight / 2.0f), g_cfg.brickWidth,
                          g_cfg.brickHeight, g_cfg.brickColor);
        });
    }

    Ball::Ball(Scene& s) : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.width / 2.0f, g_cfg.height / 2.0f};

        PhysicsBodyConfig conf = {
            .mass = 1.0f, .velocity = {250.0f, -250.0f}, .gravity = {0.0f, 0.0f}, .type = BodyType::Dynamic};
        physics_ = &add<PhysicsBody>(conf);

        col_ = &add<CircleCollider>(scene().collisions(), ColliderType::Solid, CLM::LAYER_BULLET,
                                    toLayerMask(CLM::LAYER_PLAYER | CLM::LAYER_WORLD), Vector2{0.0f, 0.0f},
                                    g_cfg.ballRadius, false);
    }

    void Ball::update(float dt) {
        RenderEntity::update(dt);
        const auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        // The ball fell off
        if (tr->position.y > g_cfg.height && !outOfFrame_) {
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
            DrawCircleV(tr->position, g_cfg.ballRadius, g_cfg.ballColor);
        });
    }


} // namespace breakout
