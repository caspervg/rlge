#pragma once
#include "breakout_level.hpp"
#include "circle_collider.hpp"
#include "physics_body.hpp"
#include "render_entity.hpp"

namespace breakout {

class Ball final : public rlge::RenderEntity {
public:
    Ball(rlge::Scene& s, const Level& level);
    void update(float dt) override;
    void draw() override;

private:
    const Level& level_;
    rlge::PhysicsBody* physics_{nullptr};
    rlge::CircleCollider* col_{nullptr};
    bool outOfFrame_ = false;
};

} // namespace breakout
