#include "vx_player.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace vox {

    namespace {
        constexpr float kMaxPitch = 89.0f * DEG2RAD;
        constexpr float kCoyoteTime = 0.12f;
        constexpr float kJumpBuffer = 0.14f;
        constexpr float kStrideLength = 2.1f;   // blocks travelled per footstep
        constexpr int kSweepIterations = 8;     // binary-search steps toward a wall
    }

    void PlayerController::spawn(World& world, const float x, const float z) {
        const int bx = static_cast<int>(std::floor(x));
        const int bz = static_cast<int>(std::floor(z));
        const int surface = world.surfaceHeight(bx, bz);
        position_ = {x, static_cast<float>(surface + 1) + 0.05f, z};
        velocity_ = {0.0f, 0.0f, 0.0f};
    }

    Vector3 PlayerController::eyePosition() const {
        // Vertical bob plus a small roll-ish lateral sway sells walking motion.
        const float bobY = std::sin(bobPhase_ * 2.0f) * 0.045f * bobAmount_;
        const float bobX = std::cos(bobPhase_) * 0.035f * bobAmount_;
        const Vector3 right{-std::sin(yaw_), 0.0f, std::cos(yaw_)};
        return {position_.x + right.x * bobX,
                position_.y + cfg.eyeHeight + bobY - landingDip_,
                position_.z + right.z * bobX};
    }

    Vector3 PlayerController::lookDir() const {
        const float cp = std::cos(pitch_);
        return {std::cos(yaw_) * cp, std::sin(pitch_), std::sin(yaw_) * cp};
    }

    float PlayerController::speedFraction() const {
        const float horizontal = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        return std::clamp(horizontal / cfg.sprintSpeed, 0.0f, 1.0f);
    }

    bool PlayerController::collides(const World& world, const Vector3 at) const {
        const float hw = cfg.playerWidth * 0.5f;
        const int x0 = static_cast<int>(std::floor(at.x - hw));
        const int x1 = static_cast<int>(std::floor(at.x + hw));
        const int y0 = static_cast<int>(std::floor(at.y));
        const int y1 = static_cast<int>(std::floor(at.y + cfg.playerHeight));
        const int z0 = static_cast<int>(std::floor(at.z - hw));
        const int z1 = static_cast<int>(std::floor(at.z + hw));
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                for (int x = x0; x <= x1; ++x) {
                    if (world.solidAt(x, y, z))
                        return true;
                }
            }
        }
        return false;
    }

    bool PlayerController::intersectsBlock(const int bx, const int by, const int bz) const {
        const float hw = cfg.playerWidth * 0.5f;
        const float minX = position_.x - hw, maxX = position_.x + hw;
        const float minY = position_.y, maxY = position_.y + cfg.playerHeight;
        const float minZ = position_.z - hw, maxZ = position_.z + hw;
        return maxX > static_cast<float>(bx) && minX < static_cast<float>(bx + 1) &&
               maxY > static_cast<float>(by) && minY < static_cast<float>(by + 1) &&
               maxZ > static_cast<float>(bz) && minZ < static_cast<float>(bz + 1);
    }

    void PlayerController::moveAxis(const World& world, const int axis, const float amount) {
        if (amount == 0.0f)
            return;
        const auto offset = [axis](Vector3 p, const float d) {
            if (axis == 0) p.x += d;
            else if (axis == 1) p.y += d;
            else p.z += d;
            return p;
        };

        if (!collides(world, offset(position_, amount))) {
            position_ = offset(position_, amount);
            return;
        }

        // Blocked: binary-search the largest free fraction so we end up flush
        // against the wall instead of a fixed step short of it.
        float lo = 0.0f;
        float hi = amount;
        for (int i = 0; i < kSweepIterations; ++i) {
            const float mid = (lo + hi) * 0.5f;
            if (collides(world, offset(position_, mid))) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
        position_ = offset(position_, lo);

        if (axis == 0) velocity_.x = 0.0f;
        else if (axis == 1) velocity_.y = 0.0f;
        else velocity_.z = 0.0f;
    }

    void PlayerController::moveHorizontal(const World& world, const float dx, const float dz) {
        const Vector3 start = position_;
        const Vector3 startVel = velocity_;

        moveAxis(world, 0, dx);
        moveAxis(world, 2, dz);

        const float wanted = std::fabs(dx) + std::fabs(dz);
        const float got = std::fabs(position_.x - start.x) + std::fabs(position_.z - start.z);
        const bool blocked = wanted > 0.0001f && got < wanted * 0.85f;
        if (!blocked || !(onGround_ || inWater_))
            return;

        // Auto step-up: retry the move lifted by one step. Without this, every
        // single-block ledge needs a deliberate jump and traversal feels sticky.
        const Vector3 blockedPos = position_;
        const Vector3 blockedVel = velocity_;
        position_ = start;
        velocity_ = startVel;

        const Vector3 lifted{start.x, start.y + cfg.stepHeight, start.z};
        if (collides(world, lifted)) {
            position_ = blockedPos;
            velocity_ = blockedVel;
            return;
        }
        position_ = lifted;
        moveAxis(world, 0, dx);
        moveAxis(world, 2, dz);
        moveAxis(world, 1, -cfg.stepHeight); // settle back onto the step

        const float steppedGot = std::fabs(position_.x - start.x) + std::fabs(position_.z - start.z);
        if (steppedGot <= got + 0.0001f) {
            position_ = blockedPos; // the step gained nothing; keep the flush slide
            velocity_ = blockedVel;
        } else {
            velocity_.x = startVel.x;
            velocity_.z = startVel.z;
        }
    }

    void PlayerController::updateFeel_(const World& world, const float dt, const float horizontalSpeed) {
        // Sound group of whatever we are standing on drives footstep audio.
        const int fx = static_cast<int>(std::floor(position_.x));
        const int fy = static_cast<int>(std::floor(position_.y - 0.1f));
        const int fz = static_cast<int>(std::floor(position_.z));
        groundSound_ = blockInfo(world.block(fx, fy, fz)).sound;

        const bool moving = onGround_ && horizontalSpeed > 0.6f;
        const float targetBob = moving ? std::min(horizontalSpeed / cfg.walkSpeed, 1.4f) : 0.0f;
        bobAmount_ += (targetBob - bobAmount_) * std::min(1.0f, dt * 8.0f);
        if (moving) {
            bobPhase_ += horizontalSpeed * dt * 1.5f;
            strideAccum_ += horizontalSpeed * dt;
            if (strideAccum_ >= kStrideLength) {
                strideAccum_ = 0.0f;
                justStepped_ = true;
            }
        } else {
            strideAccum_ = kStrideLength * 0.5f; // step promptly when walking resumes
        }

        landingDip_ = std::max(0.0f, landingDip_ - dt * 1.6f);
    }

    void PlayerController::update(World& world, const Inputs& in, const float dt) {
        justJumped_ = false;
        justSplashed_ = false;
        justStepped_ = false;
        justLanded_ = false;

        // Mouse look.
        const float invert = settings.invertY ? -1.0f : 1.0f;
        yaw_ += in.lookDelta.x * settings.mouseSensitivity;
        pitch_ -= in.lookDelta.y * settings.mouseSensitivity * invert;
        pitch_ = std::clamp(pitch_, -kMaxPitch, kMaxPitch);

        // Water state (waist and eye submersion).
        const int wx = static_cast<int>(std::floor(position_.x));
        const int wz = static_cast<int>(std::floor(position_.z));
        const int waistY = static_cast<int>(std::floor(position_.y + 0.9f));
        inWater_ = world.block(wx, waistY, wz) == Block::Water;
        const Vector3 eye{position_.x, position_.y + cfg.eyeHeight, position_.z};
        eyeInWater_ = world.block(static_cast<int>(std::floor(eye.x)),
                                  static_cast<int>(std::floor(eye.y)),
                                  static_cast<int>(std::floor(eye.z))) == Block::Water;
        if (inWater_ && !wasInWater_ && velocity_.y < -5.0f) {
            justSplashed_ = true;
        }
        wasInWater_ = inWater_;

        // Wish direction in the horizontal plane.
        const Vector3 forward{std::cos(yaw_), 0.0f, std::sin(yaw_)};
        const Vector3 right{-std::sin(yaw_), 0.0f, std::cos(yaw_)};
        Vector3 wish = Vector3Add(Vector3Scale(forward, in.moveZ), Vector3Scale(right, in.moveX));
        if (Vector3LengthSqr(wish) > 1.0f)
            wish = Vector3Normalize(wish);

        if (flying_) {
            sprinting_ = in.sprint;
            const float fly = cfg.flySpeed * (in.sprint ? 2.2f : 1.0f);
            Vector3 vel = Vector3Scale(wish, fly);
            if (in.jump) vel.y += fly;
            if (in.descend) vel.y -= fly;
            velocity_ = vel;
            moveHorizontal(world, velocity_.x * dt, velocity_.z * dt);
            moveAxis(world, 1, velocity_.y * dt);
            onGround_ = false;
            bobAmount_ = 0.0f;
            landingDip_ = std::max(0.0f, landingDip_ - dt * 1.6f);
            return;
        }

        // Sprinting only counts when actually pushing forward.
        sprinting_ = in.sprint && in.moveZ > 0.1f && !inWater_;
        const float speed = inWater_ ? cfg.swimSpeed
                          : sprinting_ ? cfg.sprintSpeed
                                       : cfg.walkSpeed;
        // Ground control is snappy, air control mushy.
        const float control = onGround_ ? 14.0f : (inWater_ ? 6.0f : 3.5f);
        velocity_.x += (wish.x * speed - velocity_.x) * std::min(1.0f, control * dt);
        velocity_.z += (wish.z * speed - velocity_.z) * std::min(1.0f, control * dt);

        // Jump forgiveness timers.
        coyoteTimer_ = onGround_ ? kCoyoteTime : std::max(0.0f, coyoteTimer_ - dt);
        if (in.jumpPressed)
            jumpBuffer_ = kJumpBuffer;
        else
            jumpBuffer_ = std::max(0.0f, jumpBuffer_ - dt);

        if (inWater_) {
            velocity_.y -= cfg.gravity * 0.22f * dt;
            velocity_.y = std::max(velocity_.y, -4.0f);
            if (in.jump) {
                velocity_.y = std::min(velocity_.y + 22.0f * dt, 3.4f); // paddle upward
            }
        } else {
            velocity_.y -= cfg.gravity * dt;
            velocity_.y = std::max(velocity_.y, -42.0f);
            if (jumpBuffer_ > 0.0f && coyoteTimer_ > 0.0f) {
                velocity_.y = cfg.jumpVelocity;
                onGround_ = false;
                coyoteTimer_ = 0.0f;
                jumpBuffer_ = 0.0f;
                justJumped_ = true;
            }
        }

        // Horizontal first (with step-up), then vertical so we land cleanly.
        moveHorizontal(world, velocity_.x * dt, velocity_.z * dt);
        const float fallSpeed = velocity_.y;
        moveAxis(world, 1, velocity_.y * dt);

        const bool wasOnGround = onGround_;
        onGround_ = fallSpeed < 0.0f && velocity_.y == 0.0f;
        if (onGround_ && !wasOnGround && fallSpeed < -4.0f) {
            justLanded_ = true;
            landingImpact_ = std::clamp(-fallSpeed / 20.0f, 0.0f, 1.0f);
            landingDip_ = landingImpact_ * 0.18f; // camera absorbs the impact
        }

        updateFeel_(world, dt, std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z));
    }

} // namespace vox
