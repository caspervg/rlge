#pragma once
#include "raylib.h"

#include "vx_config.hpp"
#include "vx_world.hpp"

namespace vox {

    // First-person character: AABB vs. voxel collision, walking/jumping,
    // swimming, and an optional fly mode. Plain class driven by the scene.
    class PlayerController {
    public:
        void spawn(World& world, float x, float z);

        struct Inputs {
            float moveX = 0.0f;    // strafe (-1 left .. +1 right)
            float moveZ = 0.0f;    // forward (-1 back .. +1 forward)
            bool jump = false;     // held
            bool sprint = false;
            bool descend = false;  // fly mode down
            Vector2 lookDelta{};   // mouse delta in pixels
        };

        void update(World& world, const Inputs& in, float dt);

        void toggleFly() { flying_ = !flying_; velocity_ = {0, 0, 0}; }
        [[nodiscard]] bool flying() const { return flying_; }
        [[nodiscard]] bool onGround() const { return onGround_; }
        [[nodiscard]] bool inWater() const { return inWater_; }
        [[nodiscard]] bool eyeInWater() const { return eyeInWater_; }
        [[nodiscard]] bool justJumped() const { return justJumped_; }
        [[nodiscard]] bool justSplashed() const { return justSplashed_; }

        [[nodiscard]] Vector3 position() const { return position_; }      // feet center
        [[nodiscard]] Vector3 eyePosition() const;
        [[nodiscard]] Vector3 lookDir() const;
        [[nodiscard]] float yaw() const { return yaw_; }
        [[nodiscard]] float pitch() const { return pitch_; }

        // True if placing a block at this cell would overlap the player.
        [[nodiscard]] bool intersectsBlock(int bx, int by, int bz) const;

    private:
        void moveAxis(const World& world, int axis, float amount);
        [[nodiscard]] bool collides(const World& world, Vector3 at) const;

        Vector3 position_{0.0f, 0.0f, 0.0f};
        Vector3 velocity_{0.0f, 0.0f, 0.0f};
        float yaw_ = 0.6f;    // radians, 0 = +X
        float pitch_ = -0.2f; // radians, clamped to +-89 deg
        bool onGround_ = false;
        bool flying_ = false;
        bool inWater_ = false;
        bool eyeInWater_ = false;
        bool wasInWater_ = false;
        bool justJumped_ = false;
        bool justSplashed_ = false;
    };

} // namespace vox
