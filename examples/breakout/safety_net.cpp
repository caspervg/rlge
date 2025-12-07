#include "safety_net.hpp"

#include "breakout_config.hpp"
#include "raylib.h"
#include "scene.hpp"

namespace breakout {

SafetyNet::SafetyNet(rlge::Scene& s, float y) : RenderEntity(s) {
    auto& tr = add<rlge::Transform>();
    tr.position = {g_cfg.viewPortWidth * 0.5f, y};

    const float thickness = 8.0f;
    
    rlge::Box2DBodyConfig bodyCfg = {
        .bodyType = b2_staticBody,
        .gravityScale = 0.0f
    };
    body_ = &add<rlge::Box2DBody>(scene().physics(), bodyCfg);

    rlge::Box2DFixtureConfig fixtureCfg = {
        .density = 1.0f,
        .friction = 0.0f,
        .restitution = 1.0f,
        .isSensor = false,
        .layer = rlge::ColliderLayerMask::LAYER_WORLD,
        .mask = rlge::ColliderLayerMask::LAYER_BULLET
    };
    body_->addBoxFixture(g_cfg.viewPortWidth, thickness, fixtureCfg);
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
