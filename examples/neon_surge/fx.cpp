#include "fx.hpp"

#include <algorithm>
#include <cmath>

#include "particle_fx.hpp"
#include "render_queue.hpp"
#include "scene.hpp"

#include "ns_config.hpp"

namespace neon {
    using namespace rlge;

    // --------------------------------------------------------- ShockwaveRing

    ShockwaveRing::ShockwaveRing(Scene& scene, const Vector2 pos, const Color color, const float maxRadius,
                                 const float duration, const float thickness) :
        RenderEntity(scene), pos_(pos), color_(color), maxRadius_(maxRadius), duration_(duration),
        thickness_(thickness) {}

    void ShockwaveRing::update(const float dt) {
        t_ += dt;
        if (t_ >= duration_) {
            destroyDeferred();
        }
    }

    void ShockwaveRing::draw() {
        const float progress = std::clamp(t_ / duration_, 0.0f, 1.0f);
        // Ease-out expansion, linear fade.
        const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
        const float radius = maxRadius_ * eased;
        const float alpha = 1.0f - progress;
        const float thickness = std::max(1.0f, thickness_ * (1.0f - progress * 0.6f));

        const Vector2 pos = pos_;
        const Color color = color_;
        rq().submitForeground(5.0f, [pos, color, radius, thickness, alpha] {
            DrawRing(pos, std::max(0.0f, radius - thickness), radius, 0.0f, 360.0f, 48,
                     Fade(color, alpha * 0.8f));
        });
    }

    // ------------------------------------------------------------ fx helpers

    namespace fx {

        void explosion(Scene& scene, const Vector2 pos, const Color color, const float power) {
            // Debris streaks.
            const auto debrisCount = static_cast<int>(16 * power);
            const BurstEmitterConfig debrisCfg{
                .maxParticles = static_cast<std::size_t>(debrisCount) + 4,
                .minLifetime = 0.25f,
                .maxLifetime = 0.6f * power,
                .minSpeed = 120.0f * power,
                .maxSpeed = 420.0f * power,
                .minSize = 1.5f,
                .maxSize = 3.5f,
                .spread = 2.0f * PI,
                .direction = 0.0f,
                .gravity = {0.0f, 0.0f},
                .startColor = color,
                .endColor = Fade(color, 0.0f)
            };
            spawnBurstEmitter(scene, pos, debrisCount, debrisCfg, [](const Particle& p) {
                const Vector2 tail{p.pos.x - p.vel.x * 0.03f, p.pos.y - p.vel.y * 0.03f};
                DrawLineEx(tail, p.pos, p.size, p.color);
            });

            // Hot white core.
            const auto coreCount = static_cast<int>(8 * power);
            const BurstEmitterConfig coreCfg{
                .maxParticles = static_cast<std::size_t>(coreCount) + 2,
                .minLifetime = 0.1f,
                .maxLifetime = 0.3f,
                .minSpeed = 20.0f,
                .maxSpeed = 120.0f * power,
                .minSize = 3.0f * power,
                .maxSize = 7.0f * power,
                .spread = 2.0f * PI,
                .gravity = {0.0f, 0.0f},
                .startColor = WHITE,
                .endColor = Fade(color, 0.0f)
            };
            spawnBurstEmitter(scene, pos, coreCount, coreCfg, [](const Particle& p) {
                DrawCircleV(p.pos, p.size, p.color);
            });

            scene.spawn<ShockwaveRing>(pos, color, 70.0f * power, 0.35f, 5.0f * power);
        }

        void sparks(Scene& scene, const Vector2 pos, const Color color, const int count, const float speed) {
            const BurstEmitterConfig sparkCfg{
                .maxParticles = static_cast<std::size_t>(count) + 2,
                .minLifetime = 0.1f,
                .maxLifetime = 0.3f,
                .minSpeed = speed * 0.4f,
                .maxSpeed = speed,
                .minSize = 1.0f,
                .maxSize = 2.5f,
                .spread = 2.0f * PI,
                .gravity = {0.0f, 0.0f},
                .startColor = color,
                .endColor = Fade(WHITE, 0.0f)
            };
            spawnBurstEmitter(scene, pos, count, sparkCfg, [](const Particle& p) {
                const Vector2 tail{p.pos.x - p.vel.x * 0.02f, p.pos.y - p.vel.y * 0.02f};
                DrawLineEx(tail, p.pos, p.size, p.color);
            });
        }

        void floatingText(Scene& scene, const Vector2 pos, const std::string& text, const Color color,
                          const float size) {
            const BurstEmitterConfig cfg{
                .maxParticles = 1,
                .minLifetime = 0.8f,
                .maxLifetime = 0.8f,
                .minSpeed = 46.0f,
                .maxSpeed = 56.0f,
                .spread = 0.35f,
                .direction = -PI / 2.0f,
                .gravity = {0.0f, -12.0f},
                .startColor = color,
                .endColor = Fade(color, 0.0f)
            };
            spawnBurstEmitter(scene, pos, 1, cfg, [text, size](const Particle& p) {
                const float life = p.life / p.totalLife;
                const float fontSize = size * (0.8f + 0.5f * life);
                const Font font = GetFontDefault();
                const Vector2 extent = MeasureTextEx(font, text.c_str(), fontSize, fontSize / 10.0f);
                const Vector2 at{p.pos.x - extent.x * 0.5f, p.pos.y - extent.y * 0.5f};
                DrawTextEx(font, text.c_str(), Vector2{at.x + 1.0f, at.y + 1.0f}, fontSize,
                           fontSize / 10.0f, Fade(BLACK, 0.5f * (p.color.a / 255.0f)));
                DrawTextEx(font, text.c_str(), at, fontSize, fontSize / 10.0f, p.color);
            });
        }

    } // namespace fx

} // namespace neon
