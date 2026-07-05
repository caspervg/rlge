#include "backdrop.hpp"

#include <cmath>

#include "runtime.hpp"
#include "scene.hpp"
#include "shader_params.hpp"

#include "ns_game.hpp"

namespace neon {
    using namespace rlge;

    namespace {
        // Tile large enough to always cover the visible view (1280x720 world units
        // plus shake/zoom margin).
        constexpr float kTileW = 2048.0f;
        constexpr float kTileH = 1280.0f;

        float positiveFmod(const float value, const float mod) {
            const float r = std::fmod(value, mod);
            return r < 0.0f ? r + mod : r;
        }
    } // namespace

    // ---------------------------------------------------------------- Nebula

    NebulaBackdrop::NebulaBackdrop(Scene& scene, NsGame* game, const Rectangle worldRect) :
        RenderEntity(scene), game_(game), rect_(worldRect) {}

    void NebulaBackdrop::update(const float dt) {
        danger_ += (targetDanger_ - danger_) * std::min(1.0f, dt * 3.0f);

        if (const auto layer = scene().layers().get(game_->nebulaLayer())) {
            if (auto* wrapper = dynamic_cast<ShaderParamsWrapper<NebulaParams>*>(
                    layer->get().shaderParams.get())) {
                auto& params = wrapper->get().params();
                params.time += dt;
                params.danger = danger_;
            }
        }
    }

    void NebulaBackdrop::draw() {
        const Texture2D& tex = game_->assets().white;
        rq().submitSprite(game_->nebulaLayer(), 0.0f, tex,
                          Rectangle{0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                          rect_, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    }

    // ------------------------------------------------------------- Starfield

    Starfield::Starfield(Scene& scene, const int starsPerTier) :
        RenderEntity(scene) {
        const Color palette[4] = {
            Color{255, 255, 255, 255},
            Color{170, 200, 255, 255},
            Color{255, 190, 235, 255},
            Color{190, 255, 245, 255},
        };

        const float factors[3] = {0.15f, 0.35f, 0.6f};
        const float sizes[3] = {1.2f, 1.8f, 2.6f};
        const unsigned char alphas[3] = {110, 160, 220};

        for (int t = 0; t < 3; ++t) {
            Tier tier;
            tier.factor = factors[t];
            tier.stars.reserve(starsPerTier);
            for (int i = 0; i < starsPerTier; ++i) {
                Star s;
                s.base = {
                    static_cast<float>(GetRandomValue(0, static_cast<int>(kTileW))),
                    static_cast<float>(GetRandomValue(0, static_cast<int>(kTileH)))
                };
                s.size = sizes[t] * (0.7f + 0.6f * static_cast<float>(GetRandomValue(0, 100)) / 100.0f);
                s.phase = static_cast<float>(GetRandomValue(0, 628)) / 100.0f;
                Color c = palette[GetRandomValue(0, 3)];
                c.a = alphas[t];
                s.color = c;
                tier.stars.push_back(s);
            }
            tiers_.push_back(std::move(tier));
        }
    }

    void Starfield::update(const float dt) { time_ += dt; }

    void Starfield::draw() {
        const View* view = scene().primaryView();
        if (!view)
            return;
        const Camera2DController* cam = view->camera2D();
        if (!cam)
            return;

        const Vector2 camTarget = cam->target();
        const float time = time_;
        // Copy what we need; the lambda runs later during the flush.
        auto tiers = tiers_;

        rq().submitBackground(0.0f, [tiers = std::move(tiers), camTarget, time] {
            for (const auto& tier : tiers) {
                for (const auto& star : tier.stars) {
                    const float wx = positiveFmod(star.base.x - camTarget.x * tier.factor, kTileW);
                    const float wy = positiveFmod(star.base.y - camTarget.y * tier.factor, kTileH);
                    const Vector2 p{
                        camTarget.x + wx - kTileW * 0.5f,
                        camTarget.y + wy - kTileH * 0.5f
                    };
                    const float twinkle = 0.65f + 0.35f * std::sin(time * 2.2f + star.phase);
                    DrawCircleV(p, star.size, Fade(star.color, twinkle * (star.color.a / 255.0f)));
                }
            }
        });
    }

    // ------------------------------------------------------------ ArenaFrame

    ArenaFrame::ArenaFrame(Scene& scene) :
        RenderEntity(scene) {}

    void ArenaFrame::update(const float dt) { time_ += dt; }

    void ArenaFrame::draw() {
        const float time = time_;
        rq().submitBackground(1.0f, [time] {
            const float w = cfg.arenaWidth;
            const float h = cfg.arenaHeight;
            const float step = cfg.gridStep;

            // Grid
            for (float x = 0.0f; x <= w + 0.5f; x += step) {
                DrawLineEx(Vector2{x, 0.0f}, Vector2{x, h}, 1.0f, pal::grid);
            }
            for (float y = 0.0f; y <= h + 0.5f; y += step) {
                DrawLineEx(Vector2{0.0f, y}, Vector2{w, y}, 1.0f, pal::grid);
            }

            // Pulsing border
            const float pulse = 0.6f + 0.4f * std::sin(time * 2.0f);
            const Rectangle border{0.0f, 0.0f, w, h};
            DrawRectangleLinesEx(border, 3.0f, Fade(pal::border, 0.9f));
            DrawRectangleLinesEx(Rectangle{-6.0f, -6.0f, w + 12.0f, h + 12.0f}, 2.0f,
                                 Fade(pal::border, 0.35f * pulse));
            DrawRectangleLinesEx(Rectangle{-14.0f, -14.0f, w + 28.0f, h + 28.0f}, 1.0f,
                                 Fade(pal::border, 0.15f * pulse));

            // Corner accents
            const float c = 46.0f;
            const Color accent = Fade(pal::player, 0.5f + 0.5f * pulse);
            const Vector2 corners[4] = {{0, 0}, {w, 0}, {w, h}, {0, h}};
            const Vector2 dirs[4][2] = {
                {{1, 0}, {0, 1}}, {{-1, 0}, {0, 1}}, {{-1, 0}, {0, -1}}, {{1, 0}, {0, -1}}
            };
            for (int i = 0; i < 4; ++i) {
                DrawLineEx(corners[i],
                           Vector2{corners[i].x + dirs[i][0].x * c, corners[i].y + dirs[i][0].y * c},
                           4.0f, accent);
                DrawLineEx(corners[i],
                           Vector2{corners[i].x + dirs[i][1].x * c, corners[i].y + dirs[i][1].y * c},
                           4.0f, accent);
            }
        });
    }

} // namespace neon
