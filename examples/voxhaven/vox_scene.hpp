#pragma once
#include <vector>

#include "camera.hpp"
#include "debug.hpp"
#include "scene.hpp"

#include "vx_blocks.hpp"
#include "vx_config.hpp"
#include "vx_player.hpp"
#include "vx_sfx.hpp"
#include "vx_world.hpp"

namespace vox {

    // Scene-local events (decouple world edits from feedback effects).
    struct BlockBroken {
        int x, y, z;
        Block block;
    };

    struct BlockPlaced {
        int x, y, z;
        Block block;
    };

    class VoxScene final : public rlge::Scene, public rlge::HasDebugOverlay {
    public:
        explicit VoxScene(rlge::Runtime& r);
        ~VoxScene() override;

        void enter() override;
        void exit() override;
        void update(float dt) override;
        void draw() override;
        void debugOverlay() override;

    private:
        enum class State { Menu, Playing, Paused };

        struct Debris {
            Vector3 pos;
            Vector3 vel;
            Color color;
            float life;
            float size;
        };

        struct Cloud {
            Vector2 pos; // world xz
            float w, d;
            float alpha;
        };

        void setupShaders_();
        void setupView_();
        void warmStart_();
        void setState_(State s);

        void updatePlaying_(float dt);
        void updateDayCycle_(float dt);
        void updateInteraction_(float dt);
        void updateDebris_(float dt);

        void drawSky_();
        void drawChunks_();
        void drawHighlight_();
        void drawDebris_();
        void drawClouds_();
        void drawHud_();

        [[nodiscard]] Color skyColor_() const;
        [[nodiscard]] float dayLight_() const;

        // --- Core state ---
        State state_ = State::Menu;
        World world_{"voxhaven.world"};
        PlayerController player_;
        Sfx sfx_;
        rlge::Camera3DController cam3_;

        Vector3 spawnPos_{8.5f, 40.0f, 8.5f};
        float menuOrbit_ = 0.0f;
        int skipLookFrames_ = 0;

        // --- Rendering ---
        Texture2D atlas_{};
        Texture2D sunTex_{};
        Texture2D moonTex_{};
        Material matLand_{};
        Material matWater_{};
        bool materialsReady_ = false;
        int locLandFogColor_ = -1, locLandFogRange_ = -1, locLandTint_ = -1, locLandCamPos_ = -1;
        int locWaterFogColor_ = -1, locWaterFogRange_ = -1, locWaterTint_ = -1, locWaterCamPos_ = -1,
            locWaterTime_ = -1;
        std::vector<Vector3> starDirs_;
        std::vector<Cloud> clouds_;

        // --- Simulation ---
        float dayTime_ = 0.22f; // 0..1, 0 = sunrise
        float worldClock_ = 0.0f;
        bool chimed_ = false;
        std::vector<Debris> debris_;

        // --- Interaction ---
        int hotbarIndex_ = 0;
        float breakProgress_ = 0.0f;
        int breakX_ = 0, breakY_ = -1, breakZ_ = 0;
        float digSoundTimer_ = 0.0f;
        float placeTimer_ = 0.0f;
        bool hasTarget_ = false;
        RayHit target_{};

        // --- Stats for the debug overlay ---
        int meshedThisFrame_ = 0;
        int drawnChunks_ = 0;
    };

} // namespace vox
