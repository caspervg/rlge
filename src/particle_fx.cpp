#include "particle_fx.hpp"

#include <algorithm>

namespace rlge {

    BurstParticleEmitterEntity::BurstParticleEmitterEntity(Scene& scene,
                                                           const BurstEmitterConfig& cfg,
                                                           BurstParticleEmitter::RenderFn renderFn,
                                                           BurstParticleEmitter::SpawnFn spawnFn) :
        RenderEntity(scene)
        , emitter_(add<BurstParticleEmitter>(cfg, std::move(renderFn))) {
        transform_ = &add<Transform>();
        if (spawnFn) {
            emitter_.setSpawnFn(std::move(spawnFn));
        }
    }

    void BurstParticleEmitterEntity::update(const float dt) {
        if (finished_)
            return;
        RenderEntity::update(dt);
        ttl_ -= dt;
        if (ttl_ <= 0.0f || emitter_.isDone()) {
            finished_ = true;
        }
    }

    void BurstParticleEmitterEntity::draw() {
        if (finished_)
            return;
        RenderEntity::draw();
    }

    BurstParticleEmitter& spawnBurstEmitter(Scene& scene,
                                            const Vector2 worldPos,
                                            const int burstCount,
                                            const BurstEmitterConfig& cfg,
                                            BurstParticleEmitter::RenderFn renderFn,
                                            BurstParticleEmitter::SpawnFn spawnFn) {
        auto& ent = scene.spawn<BurstParticleEmitterEntity>(cfg, std::move(renderFn), std::move(spawnFn));
        ent.transform().position = worldPos;
        auto& em = ent.emitter();
        em.setBurstCount(static_cast<std::size_t>(std::max(0, burstCount)));
        em.burst(static_cast<std::size_t>(burstCount));
        return em;
    }
}
