#include "pickup.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "scene.hpp"

#include "arena_scene.hpp"
#include "fx.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"
#include "player.hpp"

namespace neon {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    namespace {
        constexpr float kRadius = 13.0f;
        constexpr float kMagnetRadius = 170.0f;
    } // namespace

    Pickup::Pickup(Scene& scene, NsGame* game, const PickupType type, const Vector2 pos) :
        RenderEntity(scene), game_(game), type_(type) {
        tr_ = &add<rlge::Transform>();
        tr_->position = pos;

        // Drift away from the kill a little.
        const float angle = static_cast<float>(GetRandomValue(0, 628)) / 100.0f;
        vel_ = {std::cos(angle) * 60.0f, std::sin(angle) * 60.0f};

        add<CircleCollider>(scene.collisions(), ColliderType::Trigger, CLM::LAYER_ITEM,
                            CLM::LAYER_PLAYER, Vector2{0.0f, 0.0f}, kRadius, true);
    }

    ArenaScene& Pickup::arena() { return static_cast<ArenaScene&>(scene()); }

    Color Pickup::color() const {
        switch (type_) {
        case PickupType::RapidFire: return pal::pickupRapid;
        case PickupType::TripleShot: return pal::pickupTriple;
        case PickupType::Shield: return pal::pickupShield;
        case PickupType::Heal: return pal::pickupHeal;
        case PickupType::ScoreGem: return pal::pickupGem;
        }
        return WHITE;
    }

    void Pickup::update(const float dt) {
        RenderEntity::update(dt);
        if (collected_)
            return;

        age_ += dt;
        life_ -= dt;
        if (life_ <= 0.0f) {
            collected_ = true;
            destroyDeferred();
            return;
        }

        // Friction + magnet toward a nearby player.
        vel_ = Vector2Scale(vel_, std::max(0.0f, 1.0f - dt * 2.2f));
        if (arena().playerAlive()) {
            const Vector2 toPlayer = Vector2Subtract(arena().playerPos(), tr_->position);
            const float dist = Vector2Length(toPlayer);
            if (dist < kMagnetRadius && dist > 1.0f) {
                const float pull = (1.0f - dist / kMagnetRadius) * 1500.0f;
                const Vector2 dir = Vector2Scale(toPlayer, 1.0f / dist);
                vel_ = Vector2Add(vel_, Vector2Scale(dir, pull * dt));
            }
        }

        tr_->position.x = std::clamp(tr_->position.x + vel_.x * dt, kRadius, cfg.arenaWidth - kRadius);
        tr_->position.y = std::clamp(tr_->position.y + vel_.y * dt, kRadius, cfg.arenaHeight - kRadius);
    }

    void Pickup::collect(Player& player) {
        if (collected_)
            return;
        collected_ = true;

        player.applyPickup(type_);
        events().enqueue(PowerUpCollected{type_, tr_->position});
        fx::sparks(scene(), tr_->position, color(), 10, 300.0f);
        scene().spawn<ShockwaveRing>(tr_->position, color(), 46.0f, 0.3f, 3.0f);
        destroyDeferred();
    }

    void Pickup::draw() {
        RenderEntity::draw();
        if (collected_)
            return;

        // Blink for the last two seconds of its life.
        if (life_ < 2.0f && std::fmod(life_, 0.25f) < 0.11f)
            return;

        const Vector2 pos{tr_->position.x, tr_->position.y + std::sin(age_ * 3.2f) * 3.0f};
        const Color col = color();
        const float pulse = 0.8f + 0.2f * std::sin(age_ * 5.0f);
        const PickupType type = type_;

        const Texture2D& glowTex = game_->assets().glow;
        const float glowSize = 64.0f * pulse;
        rq().submitSprite(game_->glowLayer(), 1.5f, glowTex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(glowTex.width),
                                    static_cast<float>(glowTex.height)},
                          Rectangle{pos.x, pos.y, glowSize, glowSize},
                          Vector2{glowSize * 0.5f, glowSize * 0.5f}, 0.0f, Fade(col, 0.4f));

        const float spinDeg = age_ * 40.0f;
        rq().submitWorld(1.5f, [pos, col, pulse, type, spinDeg] {
            DrawPolyLinesEx(pos, 6, kRadius * pulse, spinDeg, 2.0f, col);

            // Type glyph.
            switch (type) {
            case PickupType::RapidFire:
                // Double chevron.
                for (int i = 0; i < 2; ++i) {
                    const float ox = -4.0f + static_cast<float>(i) * 6.0f;
                    DrawLineEx(Vector2{pos.x + ox - 2.0f, pos.y + 4.0f}, Vector2{pos.x + ox + 2.0f, pos.y},
                               2.0f, WHITE);
                    DrawLineEx(Vector2{pos.x + ox + 2.0f, pos.y}, Vector2{pos.x + ox - 2.0f, pos.y - 4.0f},
                               2.0f, WHITE);
                }
                break;
            case PickupType::TripleShot:
                DrawCircleV(Vector2{pos.x - 5.0f, pos.y + 2.0f}, 2.0f, WHITE);
                DrawCircleV(Vector2{pos.x, pos.y - 4.0f}, 2.0f, WHITE);
                DrawCircleV(Vector2{pos.x + 5.0f, pos.y + 2.0f}, 2.0f, WHITE);
                break;
            case PickupType::Shield:
                DrawRing(pos, 4.0f, 6.0f, 0.0f, 360.0f, 20, WHITE);
                break;
            case PickupType::Heal:
                DrawLineEx(Vector2{pos.x - 5.0f, pos.y}, Vector2{pos.x + 5.0f, pos.y}, 3.0f, WHITE);
                DrawLineEx(Vector2{pos.x, pos.y - 5.0f}, Vector2{pos.x, pos.y + 5.0f}, 3.0f, WHITE);
                break;
            case PickupType::ScoreGem:
                DrawPoly(pos, 4, 6.0f, 45.0f, WHITE);
                break;
            }
        });
    }

} // namespace neon
