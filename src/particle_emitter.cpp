#include "particle_emitter.hpp"

#include <algorithm>
#include <cmath>

#include "transformer.hpp"

namespace rlge {

    namespace {
        float randUnit() {
            // Simple 0..1 helper using raylib's RNG.
            return static_cast<float>(GetRandomValue(0, 1000)) / 1000.0f;
        }

        float lerp(const float a, const float b, const float t) {
            return a + (b - a) * t;
        }
    }

    ParticleEmitter::ParticleEmitter(Entity& entity, RenderFn renderFn) :
        Component(entity)
        , renderFn_(std::move(renderFn)) {}

    void ParticleEmitter::applyConfig(const ContinuousEmitterConfig& cfg) {
        localOffset_ = cfg.localOffset;
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
        enforceMaxParticles();
    }

    void ParticleEmitter::applyConfig(const BurstEmitterConfig& cfg) {
        localOffset_ = cfg.localOffset;
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
        enforceMaxParticles();
    }

    void ParticleEmitter::integrateParticles(const float dt) {
        for (auto& p : particles_) {
            p.vel.x += gravity_.x * dt;
            p.vel.y += gravity_.y * dt;
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;
            p.life -= dt;

            const float t = 1.0f - (p.life / p.totalLife); // 0..1 as it ages

            // Size over lifetime (shrink slightly towards end).
            p.size = lerp(minSize_, maxSize_, 1.0f - t);

            // Color over a lifetime.
            const auto r = static_cast<unsigned char>(lerp(startColor_.r, endColor_.r, t));
            const auto g = static_cast<unsigned char>(lerp(startColor_.g, endColor_.g, t));
            const auto b = static_cast<unsigned char>(lerp(startColor_.b, endColor_.b, t));
            const auto a = static_cast<unsigned char>(lerp(startColor_.a, endColor_.a, t));
            p.color = {r, g, b, a};
        }

        std::erase_if(particles_,[](const Particle& p) { return p.life <= 0.0f; });
    }

    void ParticleEmitter::drawParticles() {
        if (!renderFn_)
            return;

        auto& rq = entity().scene().rq();
        rq.submitWorld([this] {
            if (!renderFn_)
                return;

            for (const auto& p : particles_) {
                renderFn_(p);
            }
        });
    }

    void ParticleEmitter::spawnParticle() {
        if (particles_.size() >= maxParticles_)
            return;

        Particle p;

        const Transform* transform = entity().get<Transform>();
        const Vector2 worldOrigin = getWorldOrigin();
        const Vector2 spawnPos = spawnFn_ ? spawnFn_(worldOrigin) : worldOrigin;
        p.pos = spawnPos;

        const float life = lerp(minLifetime_, maxLifetime_, randUnit());
        p.life = life;
        p.totalLife = life;

        const float speed = lerp(minSpeed_, maxSpeed_, randUnit());
        const float local = (-spread_ * 0.5f) + spread_ * randUnit();
        const float angle = direction_ + (transform ? transform->rotation : 0.0f) + local;

        p.vel = {
            std::cos(angle) * speed,
            std::sin(angle) * speed
        };

        const float sizeScale = transform
            ? std::max(std::abs(transform->scale.x), std::abs(transform->scale.y))
            : 1.0f;
        p.size = lerp(minSize_, maxSize_, randUnit()) * sizeScale;
        p.rotation = angle;
        p.color = startColor_;

        particles_.push_back(p);
    }

    void ParticleEmitter::enforceMaxParticles() {
        if (particles_.size() <= maxParticles_)
            return;
        const auto toRemove = particles_.size() - maxParticles_;
        particles_.erase(particles_.begin(), particles_.begin() + static_cast<std::ptrdiff_t>(toRemove));
    }

    Vector2 ParticleEmitter::getWorldOrigin() const {
        if (auto* const t = entity().get<Transform>()) {
            const Vector2 scaledOffset{localOffset_.x * t->scale.x, localOffset_.y * t->scale.y};
            const Vector2 rotatedOffset = Vector2Rotate(scaledOffset, t->rotation);
            return t->position + rotatedOffset;
        }
        return localOffset_;
    }

    // Continuous emitter
    ContinuousParticleEmitter::ContinuousParticleEmitter(Entity& entity, const ContinuousEmitterConfig& cfg, RenderFn renderFn) :
        ParticleEmitter(entity, std::move(renderFn)) {
        applyConfig(cfg);
    }

    ContinuousParticleEmitter::ContinuousParticleEmitter(Entity& entity, RenderFn renderFn) :
        ContinuousParticleEmitter(entity, ContinuousEmitterConfig{}, std::move(renderFn)) {}

    ContinuousParticleEmitter::ContinuousParticleEmitter(Entity& entity) :
        ContinuousParticleEmitter(
            entity,
            ContinuousEmitterConfig{},
            [](const Particle& p) { DrawCircleV(p.pos, p.size, p.color); }) {}

    void ContinuousParticleEmitter::update(const float dt) {
        Component::update(dt);
        if (emitting_) {
            emitAccumulator_ += emitRate_ * dt;
            while (emitAccumulator_ >= 1.0f && particles_.size() < maxParticles_) {
                emitAccumulator_ -= 1.0f;
                spawnParticle();
            }
        }

        integrateParticles(dt);
    }

    void ContinuousParticleEmitter::draw() {
        drawParticles();
    }

    void ContinuousParticleEmitter::clear() {
        particles_.clear();
    }

    // Burst emitter
    BurstParticleEmitter::BurstParticleEmitter(Entity& entity, const BurstEmitterConfig& cfg, RenderFn renderFn) :
        ParticleEmitter(entity, std::move(renderFn)) {
        setConfig(cfg);
    }

    BurstParticleEmitter::BurstParticleEmitter(Entity& entity, RenderFn renderFn) :
        BurstParticleEmitter(entity, BurstEmitterConfig{}, std::move(renderFn)) {}

    BurstParticleEmitter::BurstParticleEmitter(Entity& entity) :
        BurstParticleEmitter(
            entity,
            BurstEmitterConfig{},
            [](const Particle& p) { DrawCircleV(p.pos, p.size, p.color); }) {}

    void BurstParticleEmitter::update(const float dt) {
        Component::update(dt);
        integrateParticles(dt);
        if (particles_.empty()) {
            bursting_ = false;
        }
    }

    void BurstParticleEmitter::draw() {
        drawParticles();
    }

    void BurstParticleEmitter::setConfig(const BurstEmitterConfig& cfg) {
        applyConfig(cfg);
    }

    void BurstParticleEmitter::burst(const std::size_t count) {
        bursting_ = true;
        const auto desired = static_cast<std::size_t>(count > 0 ? count : burstCount_);
        for (std::size_t i = 0; i < desired; ++i) {
            if (particles_.size() < maxParticles_) {
                spawnParticle();
            }
        }
    }

    void BurstParticleEmitter::burst() { burst(burstCount_); }

    void BurstParticleEmitter::clear() {
        particles_.clear();
        bursting_ = false;
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
