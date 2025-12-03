#include "powerup_entity.hpp"

#include "breakout_config.hpp"
#include "powerup_types.hpp"
#include "raylib.h"
#include "raymath.h"
#include "scene.hpp"

namespace breakout {

PowerUp::PowerUp(rlge::Scene& s, PowerUpType type, float x, float y)
    : RenderEntity(s)
    , type_(type)
    , config_(kPowerUpConfigs.at(type)) {
    auto& tr = add<rlge::Transform>();
    tr.position = {x, y};

    constexpr float size = 20.0f;
    collider_ = &add<rlge::BoxCollider>(
        scene().collisions(),
        rlge::ColliderType::Trigger,
        rlge::ColliderLayerMask::LAYER_ITEM,
        rlge::ColliderLayerMask::LAYER_PLAYER,
        Rectangle{-size / 2, -size / 2, size, size},
        true);
}

void PowerUp::update(float dt) {
    rlge::RenderEntity::update(dt);

    if (collected_) {
        destroyDeferred();
        return;
    }

    auto* tr = get<rlge::Transform>();
    if (!tr) return;

    tr->position.y += fallSpeed_ * dt;
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
    if (collider_) {
        collider_->unregisterCollider();
    }
}

} // namespace breakout
