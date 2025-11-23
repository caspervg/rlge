#pragma once
#include "particle_emitter.hpp"
#include "transformer.hpp"

namespace rlge {

    // Transient entity that owns a ParticleEmitter; destroys itself when the emitter finishes.
    class BurstParticleEmitterEntity final : public RenderEntity {
    public:
        BurstParticleEmitterEntity(Scene& scene,
                             const BurstEmitterConfig& cfg,
                             BurstParticleEmitter::RenderFn renderFn,
                             BurstParticleEmitter::SpawnFn spawnFn = {});

        void update(float dt) override;
        void draw() override;

        [[nodiscard]] BurstParticleEmitter& emitter() const { return emitter_; }
        [[nodiscard]] Transform& transform() const { return *transform_; }

    private:
        BurstParticleEmitter& emitter_;
        Transform* transform_{nullptr};
        float ttl_{2.0f}; // hard cap to prevent leaks on bad configs
        bool finished_{false};
    };

    // Helper to spawn a one-shot emitter at a world position and trigger a burst immediately.
    BurstParticleEmitter& spawnBurstEmitter(Scene& scene,
                                              Vector2 worldPos,
                                              int burstCount,
                                              const BurstEmitterConfig& cfg,
                                              BurstParticleEmitter::RenderFn renderFn,
                                              BurstParticleEmitter::SpawnFn spawnFn = {});
}
