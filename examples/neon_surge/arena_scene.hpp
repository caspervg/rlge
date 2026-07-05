#pragma once
#include <functional>
#include <vector>

#include "camera.hpp"
#include "debug.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "scene.hpp"

#include "ns_events.hpp"

namespace neon {
    class NsGame;
    class Player;
    class NebulaBackdrop;
    class Hud;

    // Telegraphed enemy spawn: a converging ring, then the enemy materializes.
    class SpawnPortal final : public rlge::RenderEntity {
    public:
        SpawnPortal(rlge::Scene& scene, NsGame* game, EnemyKind kind, Vector2 pos, int wave);

        void update(float dt) override;
        void draw() override;

    private:
        NsGame* game_;
        EnemyKind kind_;
        Vector2 pos_;
        int wave_;
        float t_ = 0.0f;
        bool done_ = false;
    };

    class ArenaScene final : public rlge::Scene, public rlge::HasDebugOverlay {
    public:
        ArenaScene(rlge::Runtime& r, NsGame* game);

        void enter() override;
        void update(float dt) override;
        void debugOverlay() override;

        NsGame& game() { return *game_; }
        rlge::Camera2DController& camera() { return camera_; }

        // Entities must not spawn siblings from inside update(); queue it here
        // instead and the scene flushes the queue at a safe point.
        void deferSpawn(std::function<void(rlge::Scene&)> fn);

        void addZoomKick(float amount);

        [[nodiscard]] bool playerAlive() const;
        [[nodiscard]] Vector2 playerPos() const { return lastPlayerPos_; }
        Player* player() { return player_; }

        [[nodiscard]] int wave() const { return wave_; }
        [[nodiscard]] long long score() const { return score_; }
        [[nodiscard]] int multiplier() const { return mult_; }
        [[nodiscard]] float comboFrac() const;
        [[nodiscard]] int enemiesAlive(); // non-const: walks the entity list
        [[nodiscard]] bool paused() const { return paused_; }

    private:
        enum class Phase {
            Intermission, // breather before the next wave banner
            Spawning,     // budget being spent on portals
            Fighting      // budget spent, waiting for the field to clear
        };

        void subscribeEvents_();
        void updateDirector_(float dt);
        void updateCamera_(float dt);
        void spawnGroup_();
        void spawnPortalAt_(EnemyKind kind, Vector2 pos);
        [[nodiscard]] EnemyKind pickEnemyKind_() const;
        [[nodiscard]] Vector2 pickSpawnPos_() const;
        void onEnemyKilled_(const EnemyKilled& e);
        void onPlayerDamaged_(const PlayerDamaged& e);
        void onPlayerDied_(const PlayerDied& e);
        void onPowerUpCollected_(const PowerUpCollected& e);
        void maybeDropPickup_(Vector2 pos);

        NsGame* game_;
        rlge::Camera2DController camera_;

        Player* player_ = nullptr;
        Hud* hud_ = nullptr;
        NebulaBackdrop* backdrop_ = nullptr;
        Vector2 lastPlayerPos_{0.0f, 0.0f};

        std::vector<std::function<void(rlge::Scene&)>> spawnQueue_;

        // Run state
        long long score_ = 0;
        int kills_ = 0;
        int combo_ = 0;
        int mult_ = 1;
        float comboTimer_ = 0.0f;

        // Director state
        Phase phase_ = Phase::Intermission;
        float phaseTimer_ = 1.4f;
        int wave_ = 0;
        int budget_ = 0;
        float groupTimer_ = 0.0f;

        float zoomKick_ = 0.0f;
        bool paused_ = false;
        bool runOver_ = false;
        bool godMode_ = false;
    };

} // namespace neon
