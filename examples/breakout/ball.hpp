#pragma once
#include "breakout_level.hpp"
#include "box2d_physics.hpp"
#include "particle_emitter.hpp"
#include "render_entity.hpp"

namespace breakout {

    static auto kTrailCfg = rlge::ContinuousEmitterConfig{
        .emitRate = 60.0f,
        .maxParticles = 100,
        .minLifetime = 0.1f,
        .maxLifetime = 0.25f,
        .minSpeed = 5.0f,
        .maxSpeed = 15.0f,
        .minSize = 2.0f,
        .maxSize = 4.0f,
        .spread = PI * 0.5f,
        .gravity = {0, 0},
        .startColor = WHITE,
        .endColor = Fade(WHITE, 0.0f)
    };

    class Ball final : public rlge::RenderEntity {
    public:
        Ball(rlge::Scene& s, const Level& level);
        void update(float dt) override;
        void draw() override;

    private:
        const Level& level_;
        rlge::Box2DBody* physics_{nullptr};
        bool outOfFrame_ = false;
    };

} // namespace breakout
