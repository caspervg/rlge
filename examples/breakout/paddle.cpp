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

        Box2DBodyConfig conf = {
            .bodyType = b2_kinematicBody,
            .initialVelocity = {0.0f, 0.0f},
            .gravityScale = 0.0f,
            .fixedRotation = true
        };
        physics_ = &add<Box2DBody>(scene().physics(), conf);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.0f,
            .restitution = 0.0f,
            .isSensor = false,
            .layer = CLM::LAYER_PLAYER,
            .mask = toLayerMask(static_cast<uint32_t>(CLM::LAYER_BULLET) | static_cast<uint32_t>(CLM::LAYER_ITEM))
        };
        fixture_ = physics_->addBoxFixture(level_.paddleWidth, g_cfg.paddleHeight, fixtureCfg);

        physics_->setOnCollisionEnter([this](const CollisionEvent& event) {
            onCollision(event);
        });
    }

    void Paddle::update(const float dt) {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        const auto targetWidthMult = powerUps_.paddleWidthMultiplier();
        constexpr auto lerpSpeed = 10.0f;
        smoothedWidthMult_ += (targetWidthMult - smoothedWidthMult_) * dt * lerpSpeed;

        const float effectiveWidth = level_.paddleWidth * smoothedWidthMult_;

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

        // Update Box2D body position for kinematic body
        physics_->body()->SetTransform(b2Vec2(tr->position.x, tr->position.y), 0.0f);

        // Recreate fixture if width changed significantly
        if (std::abs(smoothedWidthMult_ - 1.0f) > 0.01f) {
            if (fixture_) {
                b2Filter filter = fixture_->GetFilterData();
                float density = fixture_->GetDensity();
                float friction = fixture_->GetFriction();
                float restitution = fixture_->GetRestitution();
                bool isSensor = fixture_->IsSensor();
                
                physics_->body()->DestroyFixture(fixture_);
                fixture_ = nullptr; // Clear pointer after destruction
                
                Box2DFixtureConfig fixtureCfg = {
                    .density = density,
                    .friction = friction,
                    .restitution = restitution,
                    .isSensor = isSensor,
                    .layer = static_cast<ColliderLayerMask>(filter.categoryBits),
                    .mask = static_cast<ColliderLayerMask>(filter.maskBits)
                };
                fixture_ = physics_->addBoxFixture(effectiveWidth, g_cfg.paddleHeight, fixtureCfg);
            }
        }
    }

    void Paddle::onCollision(const CollisionEvent& event) {
        auto* otherEntity = event.getOtherEntity(&entity());
        if (!otherEntity) return;

        // Check for power-up collision
        if (auto* powerUp = dynamic_cast<PowerUp*>(otherEntity)) {
            if (!powerUp->isCollected()) {
                powerUp->collect();
                powerUps_.activate(powerUp->type());
                return;
            }
        }

        // Check for ball collision - apply deflection
        if (auto* ballBody = otherEntity->get<Box2DBody>()) {
            const auto* tr = get<rlge::Transform>();
            const auto* ballTr = otherEntity->get<rlge::Transform>();

            if (!tr || !ballTr)
                return;

            // Calculate hit offset (-1.0 to 1.0) based on paddle width
            const float effectiveWidth = level_.paddleWidth * powerUps_.paddleWidthMultiplier();
            const auto hitOffset = (ballTr->position.x - tr->position.x) / (effectiveWidth / 2.0f);

            // Calculate deflection angle
            const auto angle = hitOffset * g_cfg.maxBallPaddleDeflectionAngle * DEG2RAD;
            const auto speed = Vector2Length(ballBody->getVelocity());

            // Apply new velocity with deflection
            const auto newVel = Vector2{
                sinf(angle) * speed,
                -fabsf(cosf(angle) * speed)
            };
            ballBody->setVelocity(newVel);

            // Visual feedback tween
            scene().tweens().add(
                Tween(
                    0.08f,
                    [this](float t) {
                        scaleX_ = 1.0f + 0.2f * (1.0f - t);
                        scaleY_ = 1.0f - 0.15f * (1.0f - t);
                    },
                    easeOutQuad
                )
            );
        }
    }


    void Paddle::draw() {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<rlge::Transform>();
            if (!tr)
                return;
            const float effectiveWidth = level_.paddleWidth * smoothedWidthMult_ * scaleX_;
            const float effectiveHeight = g_cfg.paddleHeight * scaleY_;
            DrawRectangle(static_cast<int>(tr->position.x - effectiveWidth / 2.0f),
                          static_cast<int>(tr->position.y - effectiveHeight / 2.0f),
                          static_cast<int>(effectiveWidth),
                          static_cast<int>(effectiveHeight),
                          g_cfg.paddleColor
                );
        });
    }
} // namespace breakout
