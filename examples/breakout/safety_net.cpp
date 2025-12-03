#include "safety_net.hpp"

#include "breakout_config.hpp"
#include "raylib.h"
#include "scene.hpp"

namespace breakout {

SafetyNet::SafetyNet(rlge::Scene& s, float y) : RenderEntity(s) {
    auto& tr = add<rlge::Transform>();
    tr.position = {g_cfg.viewPortWidth * 0.5f, y};

    const float thickness = 8.0f;
    collider_ = &add<rlge::BoxCollider>(
        scene().collisions(),
        rlge::ColliderType::Kinematic,
        rlge::ColliderLayerMask::LAYER_WORLD,
        rlge::ColliderLayerMask::LAYER_BULLET,
        Rectangle{-g_cfg.viewPortWidth * 0.5f, -thickness * 0.5f, static_cast<float>(g_cfg.viewPortWidth), thickness},
        false);
}

void SafetyNet::update(float dt) {
    RenderEntity::update(dt);
    pulseTime_ += dt;
}

void SafetyNet::draw() {
    rq().submitWorld([this] {
        const auto* tr = get<rlge::Transform>();
        if (!tr) return;
        const float alpha = 0.6f + 0.4f * sinf(pulseTime_ * 3.0f);
        Color c = SKYBLUE;
        c.a = static_cast<unsigned char>(alpha * 255);
        DrawRectangle(0, static_cast<int>(tr->position.y - 4.0f), g_cfg.viewPortWidth, 8, c);
    });
}

} // namespace breakout
