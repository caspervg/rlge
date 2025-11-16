#include "particle_emitter.hpp"

#include <algorithm>
#include <cmath>

namespace rlge {

    namespace {
        inline float randUnit() {
            // Simple 0..1 helper using raylib's RNG.
            return static_cast<float>(GetRandomValue(0, 1000)) / 1000.0f;
        }

        inline float lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }
    }

    ParticleEmitterEntity::ParticleEmitterEntity(Scene& scene, const ParticleEmitterConfig& cfg, RenderFn renderFn)
        : RenderEntity(scene)
        , renderFn_(std::move(renderFn)) {
        applyConfig(cfg);
    }

    ParticleEmitterEntity::ParticleEmitterEntity(Scene& scene, RenderFn renderFn)
        : ParticleEmitterEntity(scene, ParticleEmitterConfig{}, std::move(renderFn)) {}

    ParticleEmitterEntity::ParticleEmitterEntity(Scene& scene)
        : ParticleEmitterEntity(
            scene,
            ParticleEmitterConfig{},
            [](const Particle& p) { DrawCircleV(p.pos, p.size, p.color); }) {}

    void ParticleEmitterEntity::applyConfig(const ParticleEmitterConfig& cfg) {
        origin_ = cfg.origin;
        emitRate_ = cfg.emitRate;
        maxParticles_ = cfg.maxParticles;
        minLifetime_ = cfg.minLifetime;
        maxLifetime_ = cfg.maxLifetime;
        minSpeed_ = cfg.minSpeed;
        maxSpeed_ = cfg.maxSpeed;
        minSize_ = cfg.minSize;
        maxSize_ = cfg.maxSize;
        spread_ = cfg.spread;
        direction_ = cfg.direction;
        gravity_ = cfg.gravity;
        startColor_ = cfg.startColor;
        endColor_ = cfg.endColor;
    }

    void ParticleEmitterEntity::spawnParticle() {
        if (particles_.size() >= maxParticles_)
            return;

        Particle p;

        const Vector2 base = origin_;
        const Vector2 spawnPos = spawnFn_ ? spawnFn_(base) : base;
        p.pos = spawnPos;

        const float life = lerp(minLifetime_, maxLifetime_, randUnit());
        p.life = life;
        p.totalLife = life;

        const float speed = lerp(minSpeed_, maxSpeed_, randUnit());
        const float local = (-spread_ * 0.5f) + spread_ * randUnit();
        const float angle = direction_ + local;

        p.vel = {
            std::cos(angle) * speed,
            std::sin(angle) * speed
        };

        p.size = lerp(minSize_, maxSize_, randUnit());
        p.rotation = angle;
        p.color = startColor_;

        particles_.push_back(p);
    }

    void ParticleEmitterEntity::update(float dt) {
        RenderEntity::update(dt);

        // Spawn new particles based on emit rate.
        emitAccumulator_ += emitRate_ * dt;
        while (emitAccumulator_ >= 1.0f && particles_.size() < maxParticles_) {
            emitAccumulator_ -= 1.0f;
            spawnParticle();
        }

        // Integrate and update particles.
        for (auto& p : particles_) {
            p.vel.x += gravity_.x * dt;
            p.vel.y += gravity_.y * dt;
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;
            p.life -= dt;

            const float t = 1.0f - (p.life / p.totalLife); // 0..1 as it ages

            // Size over lifetime (shrink slightly towards end).
            p.size = lerp(minSize_, maxSize_, 1.0f - t);

            // Color over lifetime.
            const unsigned char r = static_cast<unsigned char>(lerp(static_cast<float>(startColor_.r), static_cast<float>(endColor_.r), t));
            const unsigned char g = static_cast<unsigned char>(lerp(static_cast<float>(startColor_.g), static_cast<float>(endColor_.g), t));
            const unsigned char b = static_cast<unsigned char>(lerp(static_cast<float>(startColor_.b), static_cast<float>(endColor_.b), t));
            const unsigned char a = static_cast<unsigned char>(lerp(static_cast<float>(startColor_.a), static_cast<float>(endColor_.a), t));
            p.color = { r, g, b, a };
        }

        // Remove dead particles.
        particles_.erase(
            std::remove_if(particles_.begin(), particles_.end(),
                           [](const Particle& p) { return p.life <= 0.0f; }),
            particles_.end());
    }

    void ParticleEmitterEntity::draw() {
        rq().submitWorld([this] {
            if (!renderFn_)
                return;

            for (const auto& p : particles_) {
                renderFn_(p);
            }
        });
    }

    Vector2 spawnOnLine(const Vector2 a, const Vector2 b) {
        const float t = randUnit();
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        };
    }

    Vector2 spawnInBox(const Vector2 center, const float halfWidth, const float halfHeight) {
        const float x = lerp(center.x - halfWidth, center.x + halfWidth, randUnit());
        const float y = lerp(center.y - halfHeight, center.y + halfHeight, randUnit());
        return {x, y};
    }

    Vector2 spawnAlongBox(const Vector2 center, const float halfWidth, const float halfHeight) {
        // Choose one of the four edges, then a random point along that edge.
        const float sideChoice = randUnit();
        const float t = randUnit();

        const float left = center.x - halfWidth;
        const float right = center.x + halfWidth;
        const float top = center.y - halfHeight;
        const float bottom = center.y + halfHeight;

        if (sideChoice < 0.25f) {
            // top edge
            return {lerp(left, right, t), top};
        }
        if (sideChoice < 0.5f) {
            // bottom edge
            return {lerp(left, right, t), bottom};
        }
        if (sideChoice < 0.75f) {
            // left edge
            return {left, lerp(top, bottom, t)};
        }
        // right edge
        return {right, lerp(top, bottom, t)};
    }

    Vector2 spawnInCircle(const Vector2 center, const float radius) {
        const float r = radius * std::sqrt(randUnit());
        const float ang = 2.0f * PI * randUnit();
        return {
            center.x + r * std::cos(ang),
            center.y + r * std::sin(ang)
        };
    }

    Vector2 spawnAlongCircle(const Vector2 center, const float radius) {
        const float ang = 2.0f * PI * randUnit();
        return {
            center.x + radius * std::cos(ang),
            center.y + radius * std::sin(ang)
        };
    }
}
