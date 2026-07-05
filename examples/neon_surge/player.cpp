#include "player.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "circle_collider.hpp"
#include "raymath.h"
#include "scene.hpp"

#include "arena_scene.hpp"
#include "bullet.hpp"
#include "fx.hpp"
#include "ns_game.hpp"

namespace neon {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Player::Player(Scene& scene, NsGame* game, const Vector2 pos) :
        RenderEntity(scene), game_(game) {
        tr_ = &add<rlge::Transform>();
        tr_->position = pos;

        add<CircleCollider>(scene.collisions(), ColliderType::Trigger, CLM::LAYER_PLAYER,
                            toLayerMask(CLM::LAYER_ENEMY | CLM::LAYER_ITEM),
                            Vector2{0.0f, 0.0f}, cfg.playerRadius, true);

        const ContinuousEmitterConfig thrusterCfg{
            .localOffset = {-16.0f, 0.0f},
            .emitRate = 0.0f,
            .maxParticles = 220,
            .minLifetime = 0.2f,
            .maxLifetime = 0.45f,
            .minSpeed = 40.0f,
            .maxSpeed = 100.0f,
            .minSize = 2.5f,
            .maxSize = 5.0f,
            .spread = 0.7f,
            .direction = PI, // out the back of the ship
            .gravity = {0.0f, 0.0f},
            .startColor = Fade(pal::player, 0.8f),
            .endColor = Fade(pal::playerDark, 0.0f)
        };
        thruster_ = &add<ContinuousParticleEmitter>(thrusterCfg, [](const Particle& p) {
            DrawCircleV(p.pos, p.size, p.color);
        });
        thruster_->start();
    }

    ArenaScene& Player::arena() { return static_cast<ArenaScene&>(scene()); }

    Vector2 Player::pos() const { return tr_->position; }

    float Player::dashCooldownFrac() const {
        return 1.0f - std::clamp(dashCd_ / cfg.dashCooldown, 0.0f, 1.0f);
    }

    void Player::update(const float dt) {
        RenderEntity::update(dt);
        if (dead_)
            return;

        age_ += dt;
        iFrames_ = std::max(0.0f, iFrames_ - dt);
        rapidTimer_ = std::max(0.0f, rapidTimer_ - dt);
        tripleTimer_ = std::max(0.0f, tripleTimer_ - dt);
        dashCd_ = std::max(0.0f, dashCd_ - dt);

        handleAim_();
        handleDash_(dt);
        handleMovement_(dt);
        handleFire_(dt);

        // Ghost afterimages while dashing.
        for (auto& ghost : ghosts_)
            ghost.life -= dt;
        std::erase_if(ghosts_, [](const Ghost& g) { return g.life <= 0.0f; });
        if (dashTimer_ > 0.0f) {
            ghostAccum_ += dt;
            if (ghostAccum_ >= 0.03f) {
                ghostAccum_ = 0.0f;
                ghosts_.push_back(Ghost{tr_->position, aim_, 0.28f});
            }
        }

        // Thruster intensity follows speed.
        const float speed = Vector2Length(vel_);
        thruster_->setEmitRate(speed / cfg.playerMaxSpeed * 90.0f + (dashTimer_ > 0.0f ? 240.0f : 0.0f));
    }

    void Player::handleAim_() {
        // Right stick wins when it is deflected, otherwise aim with the mouse.
        const float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        const float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        if (rx * rx + ry * ry > 0.35f * 0.35f) {
            aim_ = std::atan2(ry, rx);
        } else {
            const Vector2 mouse = arena().camera().mouseWorldPosition();
            const Vector2 toMouse = Vector2Subtract(mouse, tr_->position);
            if (Vector2LengthSqr(toMouse) > 4.0f) {
                aim_ = std::atan2(toMouse.y, toMouse.x);
            }
        }
        tr_->rotation = aim_;
    }

    void Player::handleMovement_(const float dt) {
        const Vector2 move{
            input().axisValue(Action::MoveRight),
            input().axisValue(Action::MoveDown)
        };
        Vector2 moveDir = move;
        if (Vector2LengthSqr(moveDir) > 1.0f)
            moveDir = Vector2Normalize(moveDir);

        if (dashTimer_ > 0.0f) {
            vel_ = Vector2Scale(dashDir_, cfg.dashSpeed);
        } else {
            vel_ = Vector2Add(vel_, Vector2Scale(moveDir, cfg.playerAccel * dt));
            vel_ = Vector2Scale(vel_, std::max(0.0f, 1.0f - cfg.playerDamping * dt));
            const float speed = Vector2Length(vel_);
            if (speed > cfg.playerMaxSpeed) {
                vel_ = Vector2Scale(vel_, cfg.playerMaxSpeed / speed);
            }
        }

        tr_->position.x = std::clamp(tr_->position.x + vel_.x * dt, cfg.playerRadius,
                                     cfg.arenaWidth - cfg.playerRadius);
        tr_->position.y = std::clamp(tr_->position.y + vel_.y * dt, cfg.playerRadius,
                                     cfg.arenaHeight - cfg.playerRadius);
    }

    void Player::handleDash_(const float dt) {
        dashTimer_ = std::max(0.0f, dashTimer_ - dt);

        if (dashCd_ > 0.0f || dashTimer_ > 0.0f)
            return;
        if (!input().pressed(Action::Jump))
            return;

        const Vector2 move{
            input().axisValue(Action::MoveRight),
            input().axisValue(Action::MoveDown)
        };
        Vector2 dir = move;
        if (Vector2LengthSqr(dir) < 0.05f) {
            // No movement input: dash toward the aim direction.
            dir = {std::cos(aim_), std::sin(aim_)};
        }
        dashDir_ = Vector2Normalize(dir);
        dashTimer_ = cfg.dashTime;
        dashCd_ = cfg.dashCooldown;
        game_->assets().sfx.play("dash", 0.5f, 1.0f, 0.1f);
        arena().addZoomKick(0.02f);
    }

    void Player::handleFire_(const float dt) {
        fireTimer_ = std::max(0.0f, fireTimer_ - dt);

        const bool triggerHeld = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.2f;
        if (!(input().down(Action::Fire) || triggerHeld))
            return;
        if (fireTimer_ > 0.0f)
            return;

        fireTimer_ = rapidTimer_ > 0.0f ? cfg.fireIntervalRapid : cfg.fireInterval;
        fireOnce_();
    }

    void Player::fireOnce_() {
        const Vector2 dir{std::cos(aim_), std::sin(aim_)};
        const Vector2 muzzle = Vector2Add(tr_->position, Vector2Scale(dir, 22.0f));

        const int shots = tripleTimer_ > 0.0f ? 3 : 1;
        NsGame* game = game_;
        for (int i = 0; i < shots; ++i) {
            const float offset = shots == 1 ? 0.0f : (static_cast<float>(i) - 1.0f) * 0.16f;
            const Vector2 shotDir = Vector2Rotate(dir, offset);
            const Vector2 shotVel = Vector2Scale(shotDir, cfg.bulletSpeed);
            arena().deferSpawn([game, muzzle, shotVel](Scene& s) {
                s.spawn<Bullet>(game, muzzle, shotVel);
            });
        }

        // Recoil + report.
        vel_ = Vector2Subtract(vel_, Vector2Scale(dir, 26.0f));
        game_->assets().sfx.play("laser", 0.3f, 1.0f, 0.12f);
    }

    void Player::applyPickup(const PickupType type) {
        switch (type) {
        case PickupType::RapidFire:
            rapidTimer_ = cfg.powerUpDuration;
            break;
        case PickupType::TripleShot:
            tripleTimer_ = cfg.powerUpDuration;
            break;
        case PickupType::Shield:
            shield_ = true;
            break;
        case PickupType::Heal:
            hp_ = std::min(hp_ + 1, cfg.playerMaxHp);
            break;
        case PickupType::ScoreGem:
            // Score handled by the arena via the PowerUpCollected event.
            break;
        }
    }

    bool Player::takeHit(const Vector2 fromDir) {
        if (dead_ || iFrames_ > 0.0f || dashTimer_ > 0.0f)
            return false;

        vel_ = Vector2Add(vel_, Vector2Scale(fromDir, 460.0f));

        if (shield_) {
            shield_ = false;
            iFrames_ = 0.9f;
            game_->assets().sfx.play("shield", 0.7f, 0.8f);
            scene().spawn<ShockwaveRing>(tr_->position, pal::pickupShield, 90.0f, 0.4f, 5.0f);
            fx::sparks(scene(), tr_->position, pal::pickupShield, 14, 340.0f);
            return true;
        }

        hp_ -= 1;
        iFrames_ = cfg.playerIFrames;
        game_->assets().sfx.play("hurt", 0.8f);
        events().enqueue(PlayerDamaged{hp_, tr_->position});

        if (hp_ <= 0) {
            dead_ = true;
            fx::explosion(scene(), tr_->position, pal::player, 2.4f);
            game_->assets().sfx.play("bigboom", 0.9f, 0.8f);
            events().enqueue(PlayerDied{tr_->position});
            destroyDeferred();
        }
        return true;
    }

    void Player::draw() {
        RenderEntity::draw();
        if (dead_)
            return;

        const Vector2 pos = tr_->position;
        const float aim = aim_;

        // Blink while invulnerable.
        float alpha = 1.0f;
        if (iFrames_ > 0.0f) {
            alpha = 0.35f + 0.65f * std::fabs(std::sin(age_ * 24.0f));
        }

        const Texture2D& glowTex = game_->assets().glow;
        const float glowSize = 110.0f;
        rq().submitSprite(game_->glowLayer(), 3.0f, glowTex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(glowTex.width),
                                    static_cast<float>(glowTex.height)},
                          Rectangle{pos.x, pos.y, glowSize, glowSize},
                          Vector2{glowSize * 0.5f, glowSize * 0.5f}, 0.0f,
                          Fade(pal::player, 0.4f * alpha));

        auto shipPoints = [](const Vector2 at, const float angle) {
            const Vector2 nose = Vector2Add(at, Vector2Rotate({20.0f, 0.0f}, angle));
            const Vector2 left = Vector2Add(at, Vector2Rotate({-13.0f, -12.0f}, angle));
            const Vector2 tail = Vector2Add(at, Vector2Rotate({-7.0f, 0.0f}, angle));
            const Vector2 right = Vector2Add(at, Vector2Rotate({-13.0f, 12.0f}, angle));
            return std::array<Vector2, 4>{nose, left, tail, right};
        };

        const auto ghosts = ghosts_;
        const bool shield = shield_;
        const float shieldSpin = age_ * 2.4f;

        rq().submitWorld(4.0f, [pos, aim, alpha, ghosts, shield, shieldSpin, shipPoints] {
            // Dash afterimages.
            for (const auto& ghost : ghosts) {
                const float ghostAlpha = ghost.life / 0.28f * 0.4f;
                const auto p = shipPoints(ghost.pos, ghost.rot);
                DrawLineEx(p[0], p[1], 2.0f, Fade(pal::player, ghostAlpha));
                DrawLineEx(p[1], p[2], 2.0f, Fade(pal::player, ghostAlpha));
                DrawLineEx(p[2], p[3], 2.0f, Fade(pal::player, ghostAlpha));
                DrawLineEx(p[3], p[0], 2.0f, Fade(pal::player, ghostAlpha));
            }

            // Hull.
            const auto p = shipPoints(pos, aim);
            const Color fill{8, 40, 44, static_cast<unsigned char>(230 * alpha)};
            DrawTriangle(p[0], p[1], p[2], fill);
            DrawTriangle(p[0], p[2], p[3], fill);
            DrawLineEx(p[0], p[1], 2.5f, Fade(pal::player, alpha));
            DrawLineEx(p[1], p[2], 2.5f, Fade(pal::player, alpha));
            DrawLineEx(p[2], p[3], 2.5f, Fade(pal::player, alpha));
            DrawLineEx(p[3], p[0], 2.5f, Fade(pal::player, alpha));

            // Cockpit.
            const Vector2 cockpit = Vector2Add(pos, Vector2Rotate({6.0f, 0.0f}, aim));
            DrawCircleV(cockpit, 3.0f, Fade(WHITE, alpha));

            // Shield bubble.
            if (shield) {
                DrawRing(pos, 24.0f, 27.0f, 0.0f, 360.0f, 40, Fade(pal::pickupShield, 0.28f));
                for (int i = 0; i < 3; ++i) {
                    const float start = shieldSpin * RAD2DEG + static_cast<float>(i) * 120.0f;
                    DrawRing(pos, 24.0f, 27.0f, start, start + 60.0f, 12,
                             Fade(pal::pickupShield, 0.85f));
                }
            }
        });
    }

} // namespace neon
