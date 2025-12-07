#include "ball.hpp"

#include "breakout_config.hpp"
#include "breakout_events.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Ball::Ball(Scene& s, const Level& level) :
        RenderEntity(s), level_(level) {
        auto& tr = add<rlge::Transform>();
        tr.position = {g_cfg.viewPortWidth / 2.0f, g_cfg.viewPortHeight / 2.0f};

        Box2DBodyConfig conf = {
            .bodyType = b2_dynamicBody,
            .initialVelocity = level_.ballVelocityStart,
            .gravityScale = 0.0f,
            .linearDamping = 0.0f,
            .fixedRotation = true
        };
        physics_ = &add<Box2DBody>(scene().physics(), conf);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.0f,
            .restitution = 1.0f,
            .isSensor = false,
            .layer = CLM::LAYER_BULLET,
            .mask = toLayerMask(CLM::LAYER_PLAYER | CLM::LAYER_WORLD)
        };
        physics_->addCircleFixture(level_.ballRadius, {0.0f, 0.0f}, fixtureCfg);

        add<ContinuousParticleEmitter>(kTrailCfg,
                                       [](const Particle& p) {
                                           DrawCircleV(p.pos, p.size, p.color);
                                       }).start();
    }

    void Ball::update(const float dt) {
        RenderEntity::update(dt);
        const auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        if (tr->position.y > g_cfg.viewPortHeight && !outOfFrame_) {
            outOfFrame_ = true;
            scene().gameEvents().enqueue(BallLost{id()});
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
