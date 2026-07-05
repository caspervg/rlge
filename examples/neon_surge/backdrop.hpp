#pragma once
#include <vector>

#include "raylib.h"
#include "render_entity.hpp"

#include "ns_config.hpp"

namespace neon {
    class NsGame;

    // Fullscreen-ish quad on the "nebula" layer; the layer shader paints an
    // animated FBM nebula over it. Also owns the shader's time/danger uniforms.
    class NebulaBackdrop final : public rlge::RenderEntity {
    public:
        NebulaBackdrop(rlge::Scene& scene, NsGame* game, Rectangle worldRect);

        void update(float dt) override;
        void draw() override;

        // 0..1 - how "in trouble" the run is; tints the nebula red.
        void setDanger(const float danger) { targetDanger_ = danger; }

    private:
        NsGame* game_;
        Rectangle rect_;
        float danger_ = 0.0f;
        float targetDanger_ = 0.0f;
    };

    // Three-tier parallax starfield that tiles an infinite window around the
    // primary view's camera target.
    class Starfield final : public rlge::RenderEntity {
    public:
        Starfield(rlge::Scene& scene, int starsPerTier = 90);

        void update(float dt) override;
        void draw() override;

    private:
        struct Star {
            Vector2 base;
            float size;
            float phase;
            Color color;
        };

        struct Tier {
            float factor;  // 0 = infinitely far, 1 = camera plane
            std::vector<Star> stars;
        };

        std::vector<Tier> tiers_;
        float time_ = 0.0f;
    };

    // Arena grid + pulsing border.
    class ArenaFrame final : public rlge::RenderEntity {
    public:
        explicit ArenaFrame(rlge::Scene& scene);

        void update(float dt) override;
        void draw() override;

    private:
        float time_ = 0.0f;
    };

} // namespace neon
