#pragma once

#include "demo_shared.hpp"
#include "hud.hpp"
#include "scene.hpp"
#include "sea_creature.hpp"

namespace rlge {
    class Runtime;
    class RenderEntity;
}

namespace anim_demo {
    class AnimDemoScene final : public rlge::Scene {
    public:
        explicit AnimDemoScene(rlge::Runtime& r);

        void enter() override;
        void update(float dt) override;

    private:
        void updateWaterParams_(float dt);
        void loadVariant_(int index);

        SeaCreature* hero_{nullptr};
        Hud* hud_{nullptr};
        rlge::RenderEntity* backdrop_{nullptr};
        rlge::Camera2DController camera_;
        rlge::SpriteAtlas<AnimState>* atlas_{nullptr};
        Shader* flashShader_{nullptr};
        rlge::ShaderHandle flashShaderHandle_{rlge::InvalidShaderHandle};
        Shader* waterShader_{nullptr};
        rlge::ShaderHandle waterShaderHandle_{rlge::InvalidShaderHandle};
        rlge::ShaderParamsWrapper<WaterParams>* waterParams_{nullptr};
        rlge::LayerId bubbleLayer_{rlge::InvalidLayerId};
        int variantIndex_{0};
    };
}
