#pragma once
#include "box2d_physics.hpp"
#include "render_entity.hpp"

namespace breakout {

class SafetyNet final : public rlge::RenderEntity {
public:
    SafetyNet(rlge::Scene& s, float y);

    void update(float dt) override;
    void draw() override;

private:
    rlge::Box2DBody* body_{nullptr};
    float pulseTime_{0.0f};
};

} // namespace breakout
