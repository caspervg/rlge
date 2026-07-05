#pragma once
#include <string>

#include "raylib.h"
#include "render_entity.hpp"

namespace neon {

    // Expanding neon ring, used for explosions / spawn telegraphs / shield pops.
    class ShockwaveRing final : public rlge::RenderEntity {
    public:
        ShockwaveRing(rlge::Scene& scene, Vector2 pos, Color color, float maxRadius, float duration,
                      float thickness = 4.0f);

        void update(float dt) override;
        void draw() override;

    private:
        Vector2 pos_;
        Color color_;
        float maxRadius_;
        float duration_;
        float thickness_;
        float t_ = 0.0f;
    };

    namespace fx {
        // Chunky explosion: debris sparks + hot core flash + shockwave ring.
        // Only call from safe spawn contexts (event handlers, timers, deferred spawns).
        void explosion(rlge::Scene& scene, Vector2 pos, Color color, float power = 1.0f);

        // Tiny impact sparks (bullet hits, shield pings).
        void sparks(rlge::Scene& scene, Vector2 pos, Color color, int count, float speed = 260.0f);

        // Floating combat text that drifts upward and fades.
        void floatingText(rlge::Scene& scene, Vector2 pos, const std::string& text, Color color,
                          float size = 18.0f);
    } // namespace fx

} // namespace neon
