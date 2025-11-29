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

    struct ContinuousEmitterConfig {
        Vector2 localOffset{0.0f, 0.0f};
        float emitRate{50.0f};
        std::size_t maxParticles{500};
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
    };

    struct BurstEmitterConfig {
        Vector2 localOffset{0.0f, 0.0f};
        std::size_t maxParticles{200};
        float minLifetime{0.2f};
        float maxLifetime{0.7f};
        float minSpeed{80.0f};
        float maxSpeed{180.0f};
        float minSize{2.0f};
        float maxSize{6.0f};
        float spread{3.14159265f};
        float direction{0.0f};
        Vector2 gravity{0.0f, 50.0f};
        Color startColor{WHITE};
        Color endColor{Fade(WHITE, 0.0f)};
    };

    class ParticleEmitter : public Component {
    public:
        using RenderFn = std::function<void(const Particle&)>;
        using SpawnFn = std::function<Vector2(Vector2 origin)>;

        ParticleEmitter(Entity& entity, RenderFn renderFn);
        ParticleEmitter(Entity& entity, RenderFn renderFn, LayerId layer);
        ~ParticleEmitter() override = default;

        void setLayer(LayerId layer) { layer_ = layer; }
        [[nodiscard]] LayerId layer() const { return layer_; }

        void setLocalOffset(Vector2 localOffset) { localOffset_ = localOffset; }
        [[nodiscard]] Vector2 origin() const { return localOffset_; }

        void setMaxParticles(std::size_t max) { maxParticles_ = max; enforceMaxParticles(); }
        [[nodiscard]] std::size_t maxParticles() const { return maxParticles_; }

        void setLifetimeRange(float minL, float maxL) { minLifetime_ = minL; maxLifetime_ = maxL; }
        [[nodiscard]] float minLifetime() const { return minLifetime_; }
        [[nodiscard]] float maxLifetime() const { return maxLifetime_; }

        void setSpeedRange(float minS, float maxS) { minSpeed_ = minS; maxSpeed_ = maxS; }
        [[nodiscard]] float minSpeed() const { return minSpeed_; }
        [[nodiscard]] float maxSpeed() const { return maxSpeed_; }

        void setSizeRange(float minS, float maxS) { minSize_ = minS; maxSize_ = maxS; }
        [[nodiscard]] float minSize() const { return minSize_; }
        [[nodiscard]] float maxSize() const { return maxSize_; }

        void setSpread(const float radians) { spread_ = radians; }
        [[nodiscard]] float spread() const { return spread_; }

        void setDirection(const float radians) { direction_ = radians; }
        [[nodiscard]] float direction() const { return direction_; }

        void setGravity(Vector2 g) { gravity_ = g; }
        [[nodiscard]] Vector2 gravity() const { return gravity_; }

        void setColorRange(Color start, Color end);
        [[nodiscard]] Color startColor() const { return startColor_; }
        [[nodiscard]] Color endColor() const { return endColor_; }

        virtual void setSpawnFn(SpawnFn fn) { spawnFn_ = std::move(fn); }
        [[nodiscard]] const SpawnFn& spawnFn() const { return spawnFn_; }

    protected:
        std::vector<Particle> particles_;
        std::vector<bool> alive_;
        std::size_t liveCount_{0};

        RenderFn renderFn_;
        SpawnFn spawnFn_;

        Vector2 localOffset_{0.0f, 0.0f};
        LayerId layer_ = InvalidLayerId;

        std::size_t maxParticles_{500};    // soft cap

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

        void applyConfig(const ContinuousEmitterConfig& cfg);
        void applyConfig(const BurstEmitterConfig& cfg);

        void spawnParticle();
        void createParticleAt(size_t idx);
        void integrateParticles(float dt);
        void drawParticles();
        void enforceMaxParticles();
        [[nodiscard]] Vector2 getWorldOrigin() const;
    };

    // Continuous emitter: emits over time at a rate
    class ContinuousParticleEmitter : public ParticleEmitter {
    public:
        using RenderFn = std::function<void(const Particle&)>;
        using SpawnFn = std::function<Vector2(Vector2 origin)>;

        ContinuousParticleEmitter(Entity& entity, const ContinuousEmitterConfig& cfg, RenderFn renderFn);
        ContinuousParticleEmitter(Entity& entity, const ContinuousEmitterConfig& cfg, RenderFn renderFn, LayerId layer);
        ContinuousParticleEmitter(Entity& entity, RenderFn renderFn);
        ContinuousParticleEmitter(Entity& entity, RenderFn renderFn, LayerId layer);
        explicit ContinuousParticleEmitter(Entity& entity);

        void update(float dt) override;
        void draw() override;

        void setEmitRate(const float rate) { emitRate_ = rate; }
        [[nodiscard]] float emitRate() const { return emitRate_; }

        // Controls
        void start() { emitting_ = true; emitAccumulator_ = 0.0f; }
        void stop() { emitting_ = false; }
        void clear(); // Remove all particles

        // State
        [[nodiscard]] bool emitting() const { return emitting_; }
        [[nodiscard]] std::size_t particleCount() const { return particles_.size(); }

    private:
        float emitRate_{50.0f};            // particles per second
        bool emitting_{true};
        float emitAccumulator_{0.0f};
    };

    class BurstParticleEmitter : public ParticleEmitter {
    public:
        using RenderFn = std::function<void(const Particle&)>;
        using SpawnFn = std::function<Vector2(Vector2 origin)>;

        BurstParticleEmitter(Entity& entity, const BurstEmitterConfig& cfg, RenderFn renderFn);
        BurstParticleEmitter(Entity& entity, const BurstEmitterConfig& cfg, RenderFn renderFn, LayerId layer);
        BurstParticleEmitter(Entity& entity, RenderFn renderFn);
        BurstParticleEmitter(Entity& entity, RenderFn renderFn, LayerId layer);
        explicit BurstParticleEmitter(Entity& entity);

        void update(float dt) override;
        void draw() override;

        void setBurstCount(const std::size_t c) { burstCount_ = c; }
        [[nodiscard]] std::size_t burstCount() const { return burstCount_; }

        void setConfig(const BurstEmitterConfig& cfg);

        void burst(std::size_t count);
        void burst();
        void clear();
        [[nodiscard]] bool isDone() const { return particles_.empty() && !bursting_; }

    private:
        std::size_t burstCount_{30};
        bool bursting_{false};
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
