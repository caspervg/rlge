#include "snake_fx.hpp"

#include "particle_fx.hpp"
#include "snake_game.hpp"
#include "snake_game.hpp"

namespace snake {
    using namespace rlge;

    BurstEmitterConfig deathFxConfig() {
        return {.minLifetime = 0.2f,
                .maxLifetime = 0.5f,
                .minSpeed = 200.0f,
                .maxSpeed = 520.0f,
                .minSize = 2.0f,
                .maxSize = 5.0f,
                .spread = 2.0f * PI,
                .gravity = {0.0f, 550.0f},
                .startColor = {255, 120, 60, 255},
                .endColor = Fade(ORANGE, 0.0f)};
    }

    ParticleEmitter::RenderFn deathFxRender() {
        return [](const Particle& p) {
            // Velocity-aligned streak for death sparks.
            const float speed = Vector2Length(p.vel);
            if (speed < 1e-3f) {
                DrawCircleV(p.pos, p.size, p.color);
                return;
            }
            const float len = std::max(6.0f, speed * 0.01f);
            const Vector2 dir = Vector2Normalize(p.vel);
            const Vector2 tail{p.pos.x - dir.x * len, p.pos.y - dir.y * len};
            DrawLineEx(tail, p.pos, p.size * 0.7f, p.color);
        };
    }

    BurstEmitterConfig appleFxConfig() {
        return {.maxParticles = 60,
                .minLifetime = 0.25f,
                .maxLifetime = 0.6f,
                .minSpeed = 40.0f,
                .maxSpeed = 120.0f,
                .minSize = 2.0f,
                .maxSize = 6.0f,
                .spread = 2.0f * PI,
                .gravity = {0.0f, -60.0f},
                .startColor = GREEN,
                .endColor = Fade(LIME, 0.0f)};
    }

    ParticleEmitter::RenderFn appleFxRender() {
        return [](const Particle& p) { DrawCircleV(p.pos, p.size, p.color); };
    }

    ParticleEmitter::SpawnFn appleFxSpawn() {
        return [](const Vector2 origin) {
            return spawnInBox(origin, static_cast<float>(kTilePixels) * 0.25f,
                              static_cast<float>(kTilePixels) * 0.25f);
        };
    }

} // namespace snake
