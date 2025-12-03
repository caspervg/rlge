#pragma once
#include "box_collider.hpp"
#include "render_entity.hpp"

namespace breakout {

class SafetyNet final : public rlge::RenderEntity {
public:
    SafetyNet(rlge::Scene& s, float y);

    void update(float dt) override;
    void draw() override;

private:
    rlge::BoxCollider* collider_{nullptr};
    float pulseTime_{0.0f};
};

} // namespace breakout
