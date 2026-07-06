#include "vx_player.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace vox {

    namespace {
        constexpr float kMaxPitch = 89.0f * DEG2RAD;
    }

    void PlayerController::spawn(World& world, const float x, const float z) {
        const int bx = static_cast<int>(std::floor(x));
        const int bz = static_cast<int>(std::floor(z));
        const int surface = world.surfaceHeight(bx, bz);
        position_ = {x, static_cast<float>(surface + 1) + 0.05f, z};
        velocity_ = {0.0f, 0.0f, 0.0f};
    }

    Vector3 PlayerController::eyePosition() const {
        return {position_.x, position_.y + cfg.eyeHeight, position_.z};
    }

    Vector3 PlayerController::lookDir() const {
        const float cp = std::cos(pitch_);
        return {std::cos(yaw_) * cp, std::sin(pitch_), std::sin(yaw_) * cp};
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
        Vector3 next = position_;
        if (axis == 0) next.x += amount;
        else if (axis == 1) next.y += amount;
        else next.z += amount;

        if (!collides(world, next)) {
            position_ = next;
            return;
        }

        // Step in small increments up to the wall, then stop that axis.
        const float step = 0.02f * (amount > 0.0f ? 1.0f : -1.0f);
        float moved = 0.0f;
        while (std::fabs(moved + step) <= std::fabs(amount)) {
            Vector3 probe = position_;
            if (axis == 0) probe.x += step;
            else if (axis == 1) probe.y += step;
            else probe.z += step;
            if (collides(world, probe))
                break;
            position_ = probe;
            moved += step;
        }
        if (axis == 0) velocity_.x = 0.0f;
        else if (axis == 1) velocity_.y = 0.0f;
        else velocity_.z = 0.0f;
    }

    void PlayerController::update(World& world, const Inputs& in, const float dt) {
        justJumped_ = false;
        justSplashed_ = false;

        // Mouse look.
        yaw_ += in.lookDelta.x * cfg.mouseSensitivity;
        pitch_ -= in.lookDelta.y * cfg.mouseSensitivity;
        pitch_ = std::clamp(pitch_, -kMaxPitch, kMaxPitch);

        // Water state (waist and eye submersion).
        const int wx = static_cast<int>(std::floor(position_.x));
        const int wz = static_cast<int>(std::floor(position_.z));
        const int waistY = static_cast<int>(std::floor(position_.y + 0.9f));
        inWater_ = world.block(wx, waistY, wz) == Block::Water;
        const Vector3 eye = eyePosition();
        eyeInWater_ = world.block(static_cast<int>(std::floor(eye.x)),
                                  static_cast<int>(std::floor(eye.y)),
                                  static_cast<int>(std::floor(eye.z))) == Block::Water;
        if (inWater_ && !wasInWater_ && velocity_.y < -6.0f) {
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
            float fly = cfg.flySpeed * (in.sprint ? 2.0f : 1.0f);
            Vector3 vel = Vector3Scale(wish, fly);
            if (in.jump) vel.y += fly;
            if (in.descend) vel.y -= fly;
            velocity_ = vel;
            moveAxis(world, 0, velocity_.x * dt);
            moveAxis(world, 2, velocity_.z * dt);
            moveAxis(world, 1, velocity_.y * dt);
            onGround_ = false;
            return;
        }

        const float speed = inWater_ ? cfg.swimSpeed
                          : in.sprint ? cfg.sprintSpeed
                                      : cfg.walkSpeed;
        // Ground control is snappy, air control mushy.
        const float control = onGround_ ? 14.0f : (inWater_ ? 6.0f : 3.5f);
        velocity_.x += (wish.x * speed - velocity_.x) * std::min(1.0f, control * dt);
        velocity_.z += (wish.z * speed - velocity_.z) * std::min(1.0f, control * dt);

        if (inWater_) {
            velocity_.y -= cfg.gravity * 0.22f * dt;
            velocity_.y = std::max(velocity_.y, -4.0f);
            if (in.jump) {
                velocity_.y = std::min(velocity_.y + 22.0f * dt, 3.4f); // paddle upward
            }
        } else {
            velocity_.y -= cfg.gravity * dt;
            velocity_.y = std::max(velocity_.y, -42.0f);
            if (in.jump && onGround_) {
                velocity_.y = cfg.jumpVelocity;
                onGround_ = false;
                justJumped_ = true;
            }
        }

        // Axis-separated sweep; Y last so we land cleanly on edges.
        moveAxis(world, 0, velocity_.x * dt);
        moveAxis(world, 2, velocity_.z * dt);
        const float prevVy = velocity_.y;
        moveAxis(world, 1, velocity_.y * dt);
        onGround_ = prevVy < 0.0f && velocity_.y == 0.0f;
    }

} // namespace vox
