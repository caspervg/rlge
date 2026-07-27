#pragma once
#include <vector>

#include "camera.hpp"
#include "debug.hpp"
#include "scene.hpp"

#include "vx_blocks.hpp"
#include "vx_config.hpp"
#include "vx_hud.hpp"
#include "vx_inventory.hpp"
#include "vx_items.hpp"
#include "vx_mobs.hpp"
#include "vx_postfx.hpp"
#include "vx_player.hpp"
#include "vx_sfx.hpp"
#include "vx_viewmodel.hpp"
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
        RenderTexture2D* beginWorldRenderTarget() override;
        void afterWorldRender(RenderTexture2D* target, const std::vector<rlge::View>& views) override;
        void draw() override;
        void debugOverlay() override;

    private:
        enum class State { Menu, Playing, Paused, Settings, Inventory, Crafting };

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

        void setupFonts_();
        void setupShaders_();
        void setupView_();
        void warmStart_();
        [[nodiscard]] Vector2 findSpawnPoint_() const;
        void setState_(State s);
        [[nodiscard]] bool cursorFreeState_() const;

        void updatePlaying_(float dt);
        void updateDayCycle_(float dt);
        void updateInteraction_(float dt);
        void updateCombat_(float dt);
        void respawnPlayer_();
        void updateDebris_(float dt);
        void updateMenuActions_();
        [[nodiscard]] HudContext makeHudContext_();

        void drawSky_();
        void drawChunks_();
        void drawHighlight_();
        void drawDebris_();
        void drawMobs_();
        void drawViewModel_();
        void drawClouds_();
        void drawUi_();

        [[nodiscard]] Color skyColor_() const;
        [[nodiscard]] float dayLight_() const;

        // --- Core state ---
        State state_ = State::Menu;
        State settingsReturn_ = State::Menu;
        World world_{"voxhaven.world"};
        PlayerController player_;
        Inventory inventory_;
        Equipment equipment_;
        CraftUiState craftUi_;
        MobManager mobs_;
        PostFx postFx_;
        Sfx sfx_;
        rlge::Camera3DController cam3_;

        Vector3 spawnPos_{8.5f, 40.0f, 8.5f};
        float menuOrbit_ = 0.0f;
        int skipLookFrames_ = 0;

        // --- UI state owned by vx_hud ---
        MenuState titleMenu_;
        MenuState pauseMenu_;
        SettingsUiState settingsUi_;
        InventoryUiState inventoryUi_;
        // Player vitality; mobs are the only thing that can hurt you.
        int playerHealth_ = 20;
        int playerMaxHealth_ = 20;
        float regenTimer_ = 0.0f;
        float hurtFlash_ = 0.0f;
        float swingCooldown_ = 0.0f;
        float swingAnim_ = 0.0f;   // 0..1 through the current arm swing
        float deathTimer_ = 0.0f;

        float selectionChangedAt_ = -100.0f;
        float fpsSamples_[64] = {};
        int fpsHead_ = 0;

        // --- Rendering ---
        Texture2D atlas_{};
        Texture2D sunTex_{};
        Texture2D moonTex_{};
        Font uiFont_{};
        Font displayFont_{};
        Material matLand_{};
        Material matWater_{};
        bool materialsReady_ = false;
        int locLandFogColor_ = -1, locLandFogRange_ = -1, locLandTint_ = -1, locLandCamPos_ = -1,
            locLandDay_ = -1, locLandAmbient_ = -1;
        int locWaterFogColor_ = -1, locWaterFogRange_ = -1, locWaterTint_ = -1, locWaterCamPos_ = -1,
            locWaterTime_ = -1, locWaterDay_ = -1, locWaterAmbient_ = -1;
        std::vector<Vector3> starDirs_;
        std::vector<Cloud> clouds_;

        // Visible meshes gathered once per frame, then drawn by a single queued
        // command per layer. Submitting one command per chunk allocated a
        // std::function for every chunk, every frame.
        std::vector<Mesh> visibleOpaque_;
        std::vector<Mesh> visibleWater_;

        // --- Simulation ---
        float dayTime_ = 0.22f; // 0..1, 0 = sunrise
        float worldClock_ = 0.0f;
        bool chimed_ = false;
        std::vector<Debris> debris_;

        // --- Interaction ---
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
