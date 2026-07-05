#include "enemy.hpp"

#include <algorithm>
#include <cmath>

#include "easing.hpp"
#include "raymath.h"
#include "scene.hpp"

#include "arena_scene.hpp"
#include "fx.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"

namespace neon {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    namespace {
        struct Spec {
            float radius;
            int hp;
            float baseSpeed;
            float speedPerWave;
            float maxSpeed;
            int score;
            Color color;
            int sides;
        };

        Spec specFor(const EnemyKind kind) {
            switch (kind) {
            case EnemyKind::Chaser:   return {14.0f, 1, 180.0f, 6.0f, 320.0f, 100, pal::chaser, 3};
            case EnemyKind::Weaver:   return {13.0f, 2, 165.0f, 5.0f, 300.0f, 150, pal::weaver, 4};
            case EnemyKind::Splitter: return {26.0f, 5, 85.0f, 3.0f, 160.0f, 250, pal::splitter, 6};
            case EnemyKind::Shard:    return {9.0f, 1, 250.0f, 5.0f, 380.0f, 50, pal::shard, 3};
            case EnemyKind::Comet:    return {12.0f, 2, 330.0f, 6.0f, 460.0f, 200, pal::comet, 4};
            }
            return {14.0f, 1, 180.0f, 6.0f, 320.0f, 100, pal::chaser, 3};
        }

        float frand01() { return static_cast<float>(GetRandomValue(0, 1000)) / 1000.0f; }
    } // namespace

    Enemy::Enemy(Scene& scene, NsGame* game, const EnemyKind kind, const Vector2 pos, const int wave) :
        RenderEntity(scene), game_(game), kind_(kind) {
        const Spec spec = specFor(kind);
        radius_ = spec.radius;
        hp_ = spec.hp;
        score_ = spec.score;
        color_ = spec.color;
        sides_ = spec.sides;
        speed_ = std::min(spec.baseSpeed + spec.speedPerWave * static_cast<float>(wave), spec.maxSpeed);
        wobblePhase_ = frand01() * 2.0f * PI;

        tr_ = &add<rlge::Transform>();
        tr_->position = pos;

        col_ = &add<CircleCollider>(scene.collisions(), ColliderType::Trigger, CLM::LAYER_ENEMY,
                                    toLayerMask(CLM::LAYER_PLAYER | CLM::LAYER_BULLET),
                                    Vector2{0.0f, 0.0f}, radius_, true);
    }

    ArenaScene& Enemy::arena() { return static_cast<ArenaScene&>(scene()); }

    Vector2 Enemy::pos() const { return tr_->position; }

    void Enemy::update(const float dt) {
        RenderEntity::update(dt);
        if (dead_)
            return;

        age_ += dt;
        spawnT_ = std::min(1.0f, spawnT_ + dt / 0.35f);
        flash_ = std::max(0.0f, flash_ - dt * 6.0f);
        spin_ += dt * (kind_ == EnemyKind::Comet ? 9.0f : 1.6f);

        behave_(dt, arena().playerPos());

        tr_->position.x += vel_.x * dt;
        tr_->position.y += vel_.y * dt;

        // Keep everyone inside the arena; comets bounce, the rest just clamp.
        const float r = radius_;
        if (kind_ == EnemyKind::Comet) {
            if ((tr_->position.x < r && vel_.x < 0.0f) ||
                (tr_->position.x > cfg.arenaWidth - r && vel_.x > 0.0f)) {
                vel_.x = -vel_.x;
            }
            if ((tr_->position.y < r && vel_.y < 0.0f) ||
                (tr_->position.y > cfg.arenaHeight - r && vel_.y > 0.0f)) {
                vel_.y = -vel_.y;
            }
        }
        tr_->position.x = std::clamp(tr_->position.x, r, cfg.arenaWidth - r);
        tr_->position.y = std::clamp(tr_->position.y, r, cfg.arenaHeight - r);
    }

    void Enemy::behave_(const float dt, const Vector2 playerPos) {
        const Vector2 toPlayer = Vector2Subtract(playerPos, tr_->position);
        const float dist = Vector2Length(toPlayer);
        const Vector2 dir = dist > 1.0f ? Vector2Scale(toPlayer, 1.0f / dist) : Vector2{1.0f, 0.0f};

        switch (kind_) {
        case EnemyKind::Chaser:
        case EnemyKind::Splitter: {
            const Vector2 desired = Vector2Scale(dir, speed_);
            const float blend = std::min(1.0f, dt * 3.0f);
            vel_ = Vector2Lerp(vel_, desired, blend);
            break;
        }
        case EnemyKind::Weaver: {
            const Vector2 perp{-dir.y, dir.x};
            const float strafe = std::sin(age_ * 4.0f + wobblePhase_) * 0.85f;
            Vector2 desired = Vector2Add(dir, Vector2Scale(perp, strafe));
            desired = Vector2Scale(Vector2Normalize(desired), speed_);
            const float blend = std::min(1.0f, dt * 4.0f);
            vel_ = Vector2Lerp(vel_, desired, blend);
            break;
        }
        case EnemyKind::Shard: {
            const float jitter = std::sin(age_ * 11.0f + wobblePhase_) * 0.4f;
            const Vector2 perp{-dir.y, dir.x};
            Vector2 desired = Vector2Add(dir, Vector2Scale(perp, jitter));
            desired = Vector2Scale(Vector2Normalize(desired), speed_);
            const float blend = std::min(1.0f, dt * 6.0f);
            vel_ = Vector2Lerp(vel_, desired, blend);
            break;
        }
        case EnemyKind::Comet: {
            if (Vector2Length(vel_) < 1.0f) {
                const float wobble = (frand01() - 0.5f) * 1.2f;
                const Vector2 launch = Vector2Rotate(dir, wobble);
                vel_ = Vector2Scale(launch, speed_);
            }
            // Comets never steer; keep their speed topped up.
            vel_ = Vector2Scale(Vector2Normalize(vel_), speed_);
            break;
        }
        }
    }

    void Enemy::takeDamage(const int dmg, const Vector2 hitDir) {
        if (dead_)
            return;
        hp_ -= dmg;
        flash_ = 1.0f;
        vel_ = Vector2Add(vel_, Vector2Scale(hitDir, 90.0f));
        if (hp_ <= 0) {
            die_(true, hitDir);
        } else {
            game_->assets().sfx.play("hit", 0.5f, 1.0f, 0.15f);
        }
    }

    void Enemy::contactPlayer() {
        if (dead_)
            return;
        die_(false, {0.0f, 0.0f});
    }

    void Enemy::die_(const bool awardScore, const Vector2 hitDir) {
        dead_ = true;

        const float power = kind_ == EnemyKind::Splitter ? 1.7f : (kind_ == EnemyKind::Shard ? 0.6f : 1.0f);
        fx::explosion(scene(), tr_->position, color_, power);
        game_->assets().sfx.play(kind_ == EnemyKind::Splitter ? "bigboom" : "boom",
                                 kind_ == EnemyKind::Shard ? 0.35f : 0.6f, 1.0f, 0.2f);

        if (kind_ == EnemyKind::Splitter) {
            // Shatter into shards fanning away from the killing blow.
            const float baseAngle = std::atan2(hitDir.y, hitDir.x);
            for (int i = 0; i < 3; ++i) {
                const float angle = baseAngle + PI * 0.5f + static_cast<float>(i) * (PI * 2.0f / 3.0f);
                const Vector2 offset{std::cos(angle) * radius_, std::sin(angle) * radius_};
                scene().spawn<Enemy>(game_, EnemyKind::Shard,
                                     Vector2Add(tr_->position, offset), arena().wave());
            }
        }

        if (awardScore) {
            events().enqueue(EnemyKilled{kind_, tr_->position, score_});
        }
        destroyDeferred();
    }

    void Enemy::draw() {
        RenderEntity::draw();
        if (dead_)
            return;

        const float scale = easeOutBack(spawnT_);
        const float alpha = std::min(1.0f, spawnT_ * 2.0f);
        const Vector2 pos = tr_->position;

        // Soft halo on the glow layer (batched).
        const Texture2D& glowTex = game_->assets().glow;
        const float glowSize = radius_ * 6.0f * scale;
        rq().submitSprite(game_->glowLayer(), 1.0f, glowTex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(glowTex.width),
                                    static_cast<float>(glowTex.height)},
                          Rectangle{pos.x, pos.y, glowSize, glowSize},
                          Vector2{glowSize * 0.5f, glowSize * 0.5f}, 0.0f,
                          Fade(color_, 0.35f * alpha));

        // Vector body.
        const float r = radius_ * scale;
        const int sides = sides_;
        const Color color = color_;
        const float flash = flash_;
        float rotationDeg = spin_ * RAD2DEG;
        if (kind_ == EnemyKind::Chaser || kind_ == EnemyKind::Shard) {
            rotationDeg = std::atan2(vel_.y, vel_.x) * RAD2DEG + 90.0f;
        }
        const bool isComet = kind_ == EnemyKind::Comet;

        rq().submitWorld(2.0f, [pos, r, sides, color, flash, rotationDeg, alpha, isComet] {
            const Color fill{static_cast<unsigned char>(color.r / 4), static_cast<unsigned char>(color.g / 4),
                             static_cast<unsigned char>(color.b / 4), static_cast<unsigned char>(200 * alpha)};
            DrawPoly(pos, sides, r, rotationDeg, fill);
            DrawPolyLinesEx(pos, sides, r, rotationDeg, 2.5f, Fade(color, alpha));
            if (isComet) {
                // Second rotated square makes an 8-point star silhouette.
                DrawPolyLinesEx(pos, sides, r * 0.8f, rotationDeg + 45.0f, 2.0f, Fade(color, 0.7f * alpha));
            }
            if (flash > 0.01f) {
                DrawPoly(pos, sides, r, rotationDeg, Fade(WHITE, flash * 0.85f));
            }
        });
    }

} // namespace neon
