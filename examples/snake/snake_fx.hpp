#pragma once

#include "particle_emitter.hpp"

namespace snake {

    rlge::BurstEmitterConfig deathFxConfig();
    rlge::ParticleEmitter::RenderFn deathFxRender();

    rlge::BurstEmitterConfig appleFxConfig();
    rlge::ParticleEmitter::RenderFn appleFxRender();
    rlge::ParticleEmitter::SpawnFn appleFxSpawn();

} // namespace snake
