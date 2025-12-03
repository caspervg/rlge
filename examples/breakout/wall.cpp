#include "wall.hpp"

#include "transformer.hpp"
#include "collision/collider_types.hpp"

namespace breakout {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Wall::Wall(Scene& s, const float x, const float y, const float w, const float h) :
        Entity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {x + w / 2.0f, y + h / 2.0f};

        add<BoxCollider>(scene().collisions(), ColliderType::Kinematic, CLM::LAYER_WORLD,
                         CLM::LAYER_BULLET, Rectangle{-w / 2.0f, -h / 2.0f, w, h}, false);
    }
} // namespace breakout
