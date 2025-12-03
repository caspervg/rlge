#pragma once
#include "box_collider.hpp"
#include "breakout_level.hpp"
#include "powerup_manager.hpp"
#include "render_entity.hpp"

namespace breakout {

class Brick final : public rlge::RenderEntity {
public:
    Brick(rlge::Scene& s, const BrickConfig& config, float screenX, float screenY, PowerUpManager& powerUps);
    void onCollision(const rlge::CollisionEvent& event);
    void draw() override;

private:
    const BrickConfig& config_;
    PowerUpManager& powerUps_;
    rlge::BoxCollider* coll_{nullptr};
    bool alive_ = true;
    int maxHitPoints_{config_.hitPoints};
    int hitPoints_{config_.hitPoints};

    void spawnPowerUpsIfApplicable();
    PowerUpType getRandomPowerUpType();
};

} // namespace breakout
