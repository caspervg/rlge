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

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_staticBody,
            .gravityScale = 0.0f
        };
        auto& body = add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.0f,
            .restitution = 1.0f,
            .isSensor = false,
            .layer = CLM::LAYER_WORLD,
            .mask = CLM::LAYER_BULLET
        };
        body.addBoxFixture(w, h, fixtureCfg);
    }
} // namespace breakout
