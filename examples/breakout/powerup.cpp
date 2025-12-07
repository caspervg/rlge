#include "powerup.hpp"

#include "breakout_config.hpp"
#include "powerup_types.hpp"
#include "raylib.h"
#include "raymath.h"
#include "scene.hpp"

namespace breakout {

PowerUp::PowerUp(Scene& s, const PowerUpType type, const float x, const float y)
    : RenderEntity(s)
    , type_(type)
    , config_(kPowerUpConfigs.at(type)) {
    auto& tr = add<rlge::Transform>();
    tr.position = {x, y};

    constexpr float size = 20.0f;
    
    Box2DBodyConfig bodyCfg = {
        .bodyType = b2_dynamicBody,
        .gravityScale = 0.0f,
        .fixedRotation = true
    };
    body_ = &add<Box2DBody>(scene().physics(), bodyCfg);

    Box2DFixtureConfig fixtureCfg = {
        .density = 1.0f,
        .friction = 0.0f,
        .restitution = 0.0f,
        .isSensor = true,
        .layer = ColliderLayerMask::LAYER_ITEM,
        .mask = ColliderLayerMask::LAYER_PLAYER
    };
    body_->addBoxFixture(size, size, fixtureCfg);
    
    // Set initial downward velocity
    body_->setVelocity({0.0f, fallSpeed_});
}

void PowerUp::update(float dt) {
    RenderEntity::update(dt);

    if (collected_) {
        destroyDeferred();
        return;
    }

    auto* tr = get<rlge::Transform>();
    if (!tr) return;

    bobTime_ += dt * 4.0f;

    if (tr->position.y > g_cfg.viewPortHeight + 20.0f) {
        destroyDeferred();
    }
}

void PowerUp::draw() {
    if (collected_)
        return;

    rq().submitWorld([this] {
        const auto* tr = get<rlge::Transform>();
        if (!tr) return;

        constexpr float size = 20.0f;
        const float bobOffset = sinf(bobTime_) * bobAmplitude_;

        Rectangle rect{
            tr->position.x - size / 2.0f,
            tr->position.y - size / 2.0f + bobOffset,
            size,
            size};

        DrawRectangleRounded(rect, 0.5f, 8, config_.color);
        DrawRectangleRoundedLines(rect, 0.5f, 8, WHITE);

        const char* icon = iconForPowerUp(type_);
        const int fontSize = 12;
        int textWidth = MeasureText(icon, fontSize);
        DrawText(icon,
            static_cast<int>(tr->position.x - textWidth / 2),
            static_cast<int>(tr->position.y - fontSize / 2 + bobOffset),
            fontSize, WHITE);
    });
}

void PowerUp::collect() {
    collected_ = true;
    if (body_) {
        body_->setEnabled(false);
    }
}

} // namespace breakout
