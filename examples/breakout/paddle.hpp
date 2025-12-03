#pragma once
#include "box_collider.hpp"
#include "breakout_level.hpp"
#include "physics_body.hpp"
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
    rlge::PhysicsBody* physics_{nullptr};
    rlge::BoxCollider* coll_{nullptr};
};

} // namespace breakout
