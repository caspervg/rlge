#pragma once
#include "box2d_physics.hpp"
#include "breakout_level.hpp"
#include "powerup_manager.hpp"
#include "render_entity.hpp"

namespace breakout {

class Paddle final : public rlge::RenderEntity {
public:
    Paddle(rlge::Scene& s, const Level& level, PowerUpManager& powerUps);
    void update(float dt) override;
    void onCollision(const rlge::CollisionEvent& event);
    void draw() override;

private:
    const Level& level_;
    PowerUpManager& powerUps_;
    rlge::Box2DBody* physics_{nullptr};
    b2Fixture* fixture_{nullptr};
    float scaleX_{1.0f};
    float scaleY_{1.0f};
    float smoothedWidthMult_{1.0f};
};

} // namespace breakout
