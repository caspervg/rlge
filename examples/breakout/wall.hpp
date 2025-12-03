#pragma once
#include "box_collider.hpp"
#include "entity.hpp"

namespace breakout {

class Wall final : public rlge::Entity {
public:
    Wall(rlge::Scene& s, float x, float y, float w, float h);
};

} // namespace breakout
