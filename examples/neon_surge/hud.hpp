#pragma once
#include <string>

#include "raylib.h"
#include "render_entity.hpp"

namespace neon {
    class NsGame;
    class ArenaScene;

    // Screen-space HUD: score/combo, wave info, hearts, cooldowns, banners,
    // vignette and damage flash. Reads arena state, listens to scene events.
    class Hud final : public rlge::RenderEntity {
    public:
        Hud(rlge::Scene& scene, NsGame* game);

        void update(float dt) override;
        void draw() override;

    private:
        ArenaScene& arena();
        void showBanner_(const std::string& text, Color color);

        NsGame* game_;

        std::string bannerText_;
        Color bannerColor_ = WHITE;
        float bannerTimer_ = 0.0f;   // counts down from bannerDuration_
        float bannerDuration_ = 2.2f;

        float scorePunch_ = 0.0f;    // pops when score changes
        float damageFlash_ = 0.0f;   // red edge pulse when hit
        float time_ = 0.0f;
    };

} // namespace neon
