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

        if (tr->position.y > g_cfg.viewPortHeight && !outOfFrame_) {
            outOfFrame_ = true;
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
