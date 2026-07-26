#pragma once
#include "raylib.h"

#include "vx_blocks.hpp"
#include "vx_config.hpp"
#include "vx_world.hpp"

namespace vox {

    // First-person character: AABB vs. voxel collision, walking/jumping,
    // swimming, auto step-up, and an optional fly mode. Plain class driven by
    // the scene; it reports "feel" signals (footsteps, bob, landing) that the
    // scene turns into audio and camera motion.
    class PlayerController {
    public:
        void spawn(World& world, float x, float z);

        struct Inputs {
            float moveX = 0.0f;    // strafe (-1 left .. +1 right)
            float moveZ = 0.0f;    // forward (-1 back .. +1 forward)
            bool jump = false;     // held
            bool jumpPressed = false;
            bool sprint = false;
            bool descend = false;  // fly mode down
            Vector2 lookDelta{};   // mouse delta in pixels
        };

        void update(World& world, const Inputs& in, float dt);

        void toggleFly() { flying_ = !flying_; velocity_ = {0, 0, 0}; }
        void setFly(const bool on) { flying_ = on; velocity_ = {0, 0, 0}; }
        [[nodiscard]] bool flying() const { return flying_; }
        [[nodiscard]] bool onGround() const { return onGround_; }
        [[nodiscard]] bool inWater() const { return inWater_; }
        [[nodiscard]] bool eyeInWater() const { return eyeInWater_; }
        [[nodiscard]] bool sprinting() const { return sprinting_; }

        // One-frame feel signals, consumed by the scene for audio/FX.
        [[nodiscard]] bool justJumped() const { return justJumped_; }
        [[nodiscard]] bool justSplashed() const { return justSplashed_; }
        [[nodiscard]] bool justStepped() const { return justStepped_; }
        [[nodiscard]] bool justLanded() const { return justLanded_; }
        [[nodiscard]] float landingImpact() const { return landingImpact_; }  // 0..1
        [[nodiscard]] SoundGroup groundSound() const { return groundSound_; }

        [[nodiscard]] Vector3 position() const { return position_; }   // feet center
        [[nodiscard]] Vector3 eyePosition() const;                     // includes bob + landing dip
        [[nodiscard]] Vector3 lookDir() const;
        [[nodiscard]] float yaw() const { return yaw_; }
        [[nodiscard]] float pitch() const { return pitch_; }
        // 0..1 fraction of max ground speed, for the sprint FOV kick.
        [[nodiscard]] float speedFraction() const;

        // True if placing a block at this cell would overlap the player.
        [[nodiscard]] bool intersectsBlock(int bx, int by, int bz) const;

    private:
        void moveAxis(const World& world, int axis, float amount);
        void moveHorizontal(const World& world, float dx, float dz);
        [[nodiscard]] bool collides(const World& world, Vector3 at) const;
        void updateFeel_(const World& world, float dt, float horizontalSpeed);

        Vector3 position_{0.0f, 0.0f, 0.0f};
        Vector3 velocity_{0.0f, 0.0f, 0.0f};
        float yaw_ = 0.6f;    // radians, 0 = +X
        float pitch_ = -0.2f; // radians, clamped to +-89 deg
        bool onGround_ = false;
        bool flying_ = false;
        bool inWater_ = false;
        bool eyeInWater_ = false;
        bool wasInWater_ = false;
        bool sprinting_ = false;

        // Forgiveness windows that make jumping feel responsive rather than strict.
        float coyoteTimer_ = 0.0f;   // may still jump shortly after walking off a ledge
        float jumpBuffer_ = 0.0f;    // a jump pressed just before landing still fires

        // Feel state
        float bobPhase_ = 0.0f;
        float bobAmount_ = 0.0f;
        float strideAccum_ = 0.0f;
        float landingDip_ = 0.0f;
        float landingImpact_ = 0.0f;
        SoundGroup groundSound_ = SoundGroup::None;

        bool justJumped_ = false;
        bool justSplashed_ = false;
        bool justStepped_ = false;
        bool justLanded_ = false;
    };

} // namespace vox
