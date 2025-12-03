#include "paddle.hpp"

#include <cmath>
#include "breakout_config.hpp"
#include "breakout_events.hpp"
#include "powerup.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Paddle::Paddle(Scene& s, const Level& level, PowerUpManager& powerUps) :
        RenderEntity(s), level_(level), powerUps_(powerUps) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.viewPortWidth / 2.0f, g_cfg.viewPortHeight - 20.0f - g_cfg.paddleHeight / 2.0f};

        PhysicsBodyConfig conf = {.mass = 1.0f, .velocity = {0.f, 0.f}, .type = BodyType::Kinematic};
        physics_ = &add<PhysicsBody>(conf);

        coll_ = &add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, CLM::LAYER_PLAYER,
                                  toLayerMask(static_cast<uint32_t>(CLM::LAYER_BULLET) | static_cast<uint32_t>(CLM::LAYER_ITEM)),
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

            const auto hitOffset = (ballTr->position.x - tr->position.x) / ((level_.paddleWidth * powerUps_.paddleWidthMultiplier()) / 2.0f);

            const auto angle = hitOffset * g_cfg.maxBallPaddleDeflectionAngle * DEG2RAD;
            const auto speed = Vector2Length(ballPhysics->velocity());

            const auto newVel = Vector2{
                sinf(angle) * speed,
                -fabsf(cosf(angle) * speed)
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
} // namespace breakout
