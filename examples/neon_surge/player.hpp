#pragma once
#include <vector>

#include "particle_emitter.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"

#include "ns_config.hpp"
#include "ns_events.hpp"

namespace neon {
    class NsGame;
    class ArenaScene;

    class Player final : public rlge::RenderEntity {
    public:
        Player(rlge::Scene& scene, NsGame* game, Vector2 pos);

        void update(float dt) override;
        void draw() override;

        // Called from the arena's collision handler.
        // Returns true if the hit landed (false while invulnerable/dashing).
        bool takeHit(Vector2 fromDir);
        void applyPickup(PickupType type);

        [[nodiscard]] bool alive() const { return !dead_; }
        [[nodiscard]] Vector2 pos() const;
        [[nodiscard]] int hp() const { return hp_; }
        [[nodiscard]] bool hasShield() const { return shield_; }
        [[nodiscard]] float dashCooldownFrac() const; // 1 = ready
        [[nodiscard]] float rapidLeft() const { return rapidTimer_; }
        [[nodiscard]] float tripleLeft() const { return tripleTimer_; }

    private:
        ArenaScene& arena();
        void handleAim_();
        void handleMovement_(float dt);
        void handleDash_(float dt);
        void handleFire_(float dt);
        void fireOnce_();

        NsGame* game_;
        rlge::Transform* tr_ = nullptr;
        rlge::ContinuousParticleEmitter* thruster_ = nullptr;

        Vector2 vel_{0.0f, 0.0f};
        float aim_ = 0.0f; // radians

        int hp_ = cfg.playerMaxHp;
        bool shield_ = false;
        float iFrames_ = 0.0f;

        float fireTimer_ = 0.0f;
        float rapidTimer_ = 0.0f;
        float tripleTimer_ = 0.0f;

        float dashTimer_ = 0.0f;
        float dashCd_ = 0.0f;
        Vector2 dashDir_{1.0f, 0.0f};

        struct Ghost {
            Vector2 pos;
            float rot;
            float life;
        };
        std::vector<Ghost> ghosts_;
        float ghostAccum_ = 0.0f;
        float age_ = 0.0f;

        bool dead_ = false;
    };

} // namespace neon
