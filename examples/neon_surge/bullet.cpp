#include "bullet.hpp"

#include "raymath.h"
#include "scene.hpp"

#include "enemy.hpp"
#include "fx.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"

namespace neon {
    using namespace rlge;
    using CLM = ColliderLayerMask;

    Bullet::Bullet(Scene& scene, NsGame* game, const Vector2 pos, const Vector2 vel) :
        RenderEntity(scene), game_(game), vel_(vel), life_(cfg.bulletLife) {
        tr_ = &add<rlge::Transform>();
        tr_->position = pos;

        add<CircleCollider>(scene.collisions(), ColliderType::Trigger, CLM::LAYER_BULLET,
                            CLM::LAYER_ENEMY, Vector2{0.0f, 0.0f}, cfg.bulletRadius, true);
    }

    void Bullet::update(const float dt) {
        RenderEntity::update(dt);
        if (spent_)
            return;

        tr_->position.x += vel_.x * dt;
        tr_->position.y += vel_.y * dt;

        life_ -= dt;
        const Vector2 p = tr_->position;
        const bool outside = p.x < -40.0f || p.y < -40.0f ||
                             p.x > cfg.arenaWidth + 40.0f || p.y > cfg.arenaHeight + 40.0f;
        if (life_ <= 0.0f || outside) {
            spent_ = true;
            destroyDeferred();
        }
    }

    void Bullet::hitEnemy(Enemy& enemy) {
        if (spent_)
            return;
        spent_ = true;

        const Vector2 dir = Vector2Normalize(vel_);
        enemy.takeDamage(1, dir);
        fx::sparks(scene(), tr_->position, pal::bullet, 5, 220.0f);
        destroyDeferred();
    }

    void Bullet::draw() {
        RenderEntity::draw();
        if (spent_)
            return;

        const Vector2 pos = tr_->position;
        const Vector2 vel = vel_;

        const Texture2D& glowTex = game_->assets().glow;
        const float glowSize = 34.0f;
        rq().submitSprite(game_->glowLayer(), 2.0f, glowTex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(glowTex.width),
                                    static_cast<float>(glowTex.height)},
                          Rectangle{pos.x, pos.y, glowSize, glowSize},
                          Vector2{glowSize * 0.5f, glowSize * 0.5f}, 0.0f,
                          Fade(pal::bullet, 0.5f));

        rq().submitWorld(3.0f, [pos, vel] {
            const Vector2 tail{pos.x - vel.x * 0.016f, pos.y - vel.y * 0.016f};
            DrawLineEx(tail, pos, 3.5f, Fade(pal::bullet, 0.85f));
            DrawCircleV(pos, cfg.bulletRadius * 0.8f, WHITE);
        });
    }

} // namespace neon
