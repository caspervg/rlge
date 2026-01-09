#pragma once

#include "animation.hpp"
#include "asset_atlas.hpp"
#include "demo_shared.hpp"
#include "particle_emitter.hpp"
#include "render_entity.hpp"
#include "shader_effect.hpp"
#include "sprite_atlas_sprite.hpp"

namespace anim_demo {
    class SeaCreature final : public rlge::RenderEntity {
    public:
        SeaCreature(rlge::Scene& scene, rlge::SpriteAtlas<AnimState>& atlas, Shader flashShader,
            rlge::ShaderHandle flashHandle, rlge::Camera2DController& camera, rlge::LayerId bubbleLayer);

        void update(float dt) override;
        void draw() override;

        [[nodiscard]] AnimState state() const;
        [[nodiscard]] const std::string& lastEvent() const;
        [[nodiscard]] float eventTimer() const;

    private:
        void registerClips_();
        void handleInput_(float dt);
        void triggerFlash_(Vector3 color, float duration, float intensity);
        void updateFlash_(float dt);
        void pushEvent_(const char* label);

        rlge::AnimationStateMachine<AnimState>* anim_{nullptr};
        rlge::ShaderEffect<FlashParams>* flash_{nullptr};
        rlge::ContinuousParticleEmitter* bubbleEmitter_{nullptr};
        rlge::SpriteAtlas<AnimState>& atlas_;
        rlge::Camera2DController& camera_;
        bool facingLeft_{false};
        bool queueIdle_{false};
        bool moving_{false};
        float flashTimer_{0.0f};
        float flashDuration_{0.18f};
        float flashMaxIntensity_{0.85f};
        std::string lastEvent_{"None"};
        float eventTimer_{0.0f};
        float eventLabelDuration_{1.25f};
    };
}
