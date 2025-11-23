#pragma once

#include <functional>
#include <vector>

#include "raylib.h"

#include "render_entity.hpp"

namespace rlge {

    struct Particle {
        Vector2 pos{};
        Vector2 vel{};
        float life{0.0f};       // remaining
        float totalLife{0.0f};  // initial
        float size{0.0f};
        float rotation{0.0f};
        Color color{WHITE};
    };

    struct ParticleEmitterConfig {
        Vector2 localOffset{0.0f, 0.0f};
        float emitRate{50.0f};
        std::size_t maxParticles{500};
        std::size_t oneShotCount{0};
        float minLifetime{0.4f};
        float maxLifetime{1.0f};
        float minSpeed{50.0f};
        float maxSpeed{150.0f};
        float minSize{2.0f};
        float maxSize{5.0f};
        float spread{3.14159265f};
        float direction{0.0f};
        Vector2 gravity{0.0f, 50.0f};
        Color startColor{WHITE};
        Color endColor{Fade(WHITE, 0.0f)};
        bool oneShot{false};
    };

    class ParticleEmitter : public Component {
    public:
        using RenderFn = std::function<void(const Particle&)>;
        using SpawnFn = std::function<Vector2(Vector2 origin)>;

        ParticleEmitter(Entity& entity, const ParticleEmitterConfig& cfg, RenderFn renderFn);
        ParticleEmitter(Entity& entity, RenderFn renderFn);
        explicit ParticleEmitter(Entity& entity);

        void update(float dt) override;
        void draw() override;

        // Configuration helpers
        void setLocalOffset(const Vector2 localOffset) { localOffset_ = localOffset; }
        [[nodiscard]] Vector2 origin() const { return localOffset_; }

        void setEmitRate(const float rate) { emitRate_ = rate; }
        [[nodiscard]] float emitRate() const { return emitRate_; }

        void setMaxParticles(const std::size_t max) { maxParticles_ = max; enforceMaxParticles(); }
        [[nodiscard]] std::size_t maxParticles() const { return maxParticles_; }

        void setLifetimeRange(const float minL, const float maxL) {
            minLifetime_ = minL;
            maxLifetime_ = maxL;
        }
        [[nodiscard]] float minLifetime() const { return minLifetime_; }
        [[nodiscard]] float maxLifetime() const { return maxLifetime_; }

        void setSpeedRange(const float minS, const float maxS) {
            minSpeed_ = minS;
            maxSpeed_ = maxS;
        }
        [[nodiscard]] float minSpeed() const { return minSpeed_; }
        [[nodiscard]] float maxSpeed() const { return maxSpeed_; }

        void setSizeRange(const float minS, const float maxS) {
            minSize_ = minS;
            maxSize_ = maxS;
        }
        [[nodiscard]] float minSize() const { return minSize_; }
        [[nodiscard]] float maxSize() const { return maxSize_; }

        void setSpread(const float radians) { spread_ = radians; }
        [[nodiscard]] float spread() const { return spread_; }

        void setDirection(const float radians) { direction_ = radians; }
        [[nodiscard]] float direction() const { return direction_; }

        void setGravity(const Vector2 g) { gravity_ = g; }
        [[nodiscard]] Vector2 gravity() const { return gravity_; }

        void setColorRange(Color start, Color end);
        [[nodiscard]] Color startColor() const { return startColor_; }
        [[nodiscard]] Color endColor() const { return endColor_; }

        void setSpawnFn(SpawnFn fn) { spawnFn_ = std::move(fn); }
        [[nodiscard]] const SpawnFn& spawnFn() const { return spawnFn_; }

        void setOneShot(const bool oneShot) { oneShot_ = oneShot; }
        [[nodiscard]] bool oneShot() const { return oneShot_; }
        void setOneShotCount(std::size_t count) { oneShotCount_ = count; oneShotRemaining_ = count; }
        [[nodiscard]] std::size_t oneShotCount() const { return oneShotCount_; }

        // Controls
        void start() { emitting_ = true; emitAccumulator_ = 0.0f; oneShotRemaining_ = oneShotCount_; }
        void stop() { emitting_ = false; }
        void burst(int count); // Emit 'count' particles immediately (respects one-shot remaining)
        void clear(); // Remove all particles

        // State
        [[nodiscard]] bool emitting() const { return emitting_; }
        [[nodiscard]] bool isDone() const { return oneShot_ && !emitting_ && particles_.empty(); }
        [[nodiscard]] std::size_t particleCount() const { return particles_.size(); }

    private:
        std::vector<Particle> particles_;
        RenderFn renderFn_;
        SpawnFn spawnFn_;

        Vector2 localOffset_{0.0f, 0.0f};

        float emitRate_{50.0f};            // particles per second
        std::size_t maxParticles_{500};    // soft cap
        std::size_t oneShotCount_{0};
        std::size_t oneShotRemaining_{0};

        float minLifetime_{0.4f};
        float maxLifetime_{1.0f};
        float minSpeed_{50.0f};
        float maxSpeed_{150.0f};
        float minSize_{2.0f};
        float maxSize_{5.0f};
        float spread_{3.14159265f};        // radians, total cone angle
        float direction_{0.0f};            // radians, cone center direction
        Vector2 gravity_{0.0f, 50.0f};

        Color startColor_{WHITE};
        Color endColor_{Fade(WHITE, 0.0f)};

        bool oneShot_{false};
        bool emitting_{true};
        float emitAccumulator_{0.0f};

        void applyConfig(const ParticleEmitterConfig& cfg);
        void spawnParticle();
        void enforceMaxParticles();
        [[nodiscard]] Vector2 getWorldOrigin() const;
    };

    inline void ParticleEmitter::setColorRange(const Color start, const Color end) {
        startColor_ = start;
        endColor_ = end;
    }

    // Generic spawn helpers for common shapes. These are optional utilities
    // that can be used when wiring SpawnFn lambdas in game/demo code.
    Vector2 spawnOnLine(Vector2 a, Vector2 b);
    Vector2 spawnInBox(Vector2 center, float halfWidth, float halfHeight);
    Vector2 spawnAlongBox(Vector2 center, float halfWidth, float halfHeight);
    Vector2 spawnInCircle(Vector2 center, float radius);
    Vector2 spawnAlongCircle(Vector2 center, float radius);
}
