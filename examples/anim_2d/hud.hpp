#pragma once

#include "render_entity.hpp"
#include "sea_creature.hpp"

namespace anim_demo {
    class Hud final : public rlge::RenderEntity {
    public:
        Hud(rlge::Scene& scene, SeaCreature& hero);

        void draw() override;

    private:
        SeaCreature& hero_;
    };
}
