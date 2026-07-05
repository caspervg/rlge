#include "hud.hpp"

#include <algorithm>
#include <cmath>

#include "render_queue.hpp"
#include "scene.hpp"

#include "arena_scene.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"
#include "player.hpp"

namespace neon {
    using namespace rlge;

    namespace {
        void drawLabel(const Vector2 at, const char* text, const float size, const Color color) {
            DrawTextEx(GetFontDefault(), text, at, size, size / 10.0f, color);
        }

        Vector2 measure(const char* text, const float size) {
            return MeasureTextEx(GetFontDefault(), text, size, size / 10.0f);
        }
    } // namespace

    Hud::Hud(Scene& scene, NsGame* game) :
        RenderEntity(scene), game_(game) {
        events().subscribe<WaveStarted>([this](const WaveStarted& e) {
            showBanner_(TextFormat("WAVE %d", e.wave), pal::player);
        });
        events().subscribe<WaveCleared>([this](const WaveCleared& e) {
            showBanner_(TextFormat("WAVE %d CLEARED", e.wave), pal::pickupHeal);
        });
        events().subscribe<ScoreChanged>([this](const ScoreChanged&) {
            scorePunch_ = 1.0f;
        });
        events().subscribe<PlayerDamaged>([this](const PlayerDamaged&) {
            damageFlash_ = 1.0f;
        });
        events().subscribe<PlayerDied>([this](const PlayerDied&) {
            showBanner_("SHIP LOST", pal::chaser);
        });
    }

    ArenaScene& Hud::arena() { return static_cast<ArenaScene&>(scene()); }

    void Hud::showBanner_(const std::string& text, const Color color) {
        bannerText_ = text;
        bannerColor_ = color;
        bannerTimer_ = bannerDuration_;
    }

    void Hud::update(const float dt) {
        RenderEntity::update(dt);
        time_ += dt;
        bannerTimer_ = std::max(0.0f, bannerTimer_ - dt);
        scorePunch_ = std::max(0.0f, scorePunch_ - dt * 3.5f);
        damageFlash_ = std::max(0.0f, damageFlash_ - dt * 2.0f);
    }

    void Hud::draw() {
        RenderEntity::draw();

        const View* view = scene().primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;

        ArenaScene& scene = arena();
        Player* player = scene.player();

        const long long score = scene.score();
        const int mult = scene.multiplier();
        const float comboFrac = scene.comboFrac();
        const int wave = scene.wave();
        const int hostiles = scene.enemiesAlive();
        const long long best = std::max(game_->highScore(), score);
        const int hp = player ? player->hp() : 0;
        const bool shield = player && player->hasShield();
        const float dashFrac = player ? player->dashCooldownFrac() : 0.0f;
        const float rapidLeft = player ? player->rapidLeft() : 0.0f;
        const float tripleLeft = player ? player->tripleLeft() : 0.0f;

        const std::string bannerText = bannerText_;
        const Color bannerColor = bannerColor_;
        const float bannerTimer = bannerTimer_;
        const float bannerDuration = bannerDuration_;
        const float scorePunch = scorePunch_;
        const float damageFlash = damageFlash_;
        const float time = time_;
        const bool paused = scene.paused();
        const Texture2D vignetteTex = game_->assets().vignette;

        rq().submitUI([=] {
            const float x0 = vp.x;
            const float y0 = vp.y;

            // --- Vignette + damage pulse ---
            DrawTexturePro(vignetteTex,
                           Rectangle{0, 0, static_cast<float>(vignetteTex.width),
                                     static_cast<float>(vignetteTex.height)},
                           vp, Vector2{0, 0}, 0.0f, Fade(WHITE, 0.85f));
            float redEdge = damageFlash * 0.55f;
            if (hp == 1) {
                redEdge = std::max(redEdge, 0.25f + 0.15f * std::sin(time * 5.0f));
            }
            if (redEdge > 0.01f) {
                DrawTexturePro(vignetteTex,
                               Rectangle{0, 0, static_cast<float>(vignetteTex.width),
                                         static_cast<float>(vignetteTex.height)},
                               vp, Vector2{0, 0}, 0.0f, Fade(Color{255, 30, 60, 255}, redEdge));
            }

            // --- Score block (top-left) ---
            drawLabel({x0 + 24, y0 + 18}, "SCORE", 16, pal::hudDim);
            const float scoreSize = 36.0f * (1.0f + 0.25f * scorePunch);
            drawLabel({x0 + 24, y0 + 36}, TextFormat("%lld", score), scoreSize, pal::hudText);

            if (mult > 1) {
                const Color multColor = mult >= 6 ? pal::chaser : (mult >= 3 ? pal::comet : pal::hudText);
                drawLabel({x0 + 24, y0 + 78}, TextFormat("x%d", mult), 26, multColor);
            }
            // Combo decay bar.
            if (comboFrac > 0.0f) {
                DrawRectangle(static_cast<int>(x0 + 24), static_cast<int>(y0 + 108),
                              static_cast<int>(130.0f * comboFrac), 5, Fade(pal::comet, 0.9f));
                DrawRectangleLines(static_cast<int>(x0 + 24), static_cast<int>(y0 + 108), 130, 5,
                                   Fade(pal::hudDim, 0.5f));
            }

            // --- Best (top-center) ---
            {
                const auto text = TextFormat("BEST %lld", best);
                const Vector2 ext = measure(text, 16);
                drawLabel({x0 + vp.width * 0.5f - ext.x * 0.5f, y0 + 18}, text, 16, pal::hudDim);
            }

            // --- Wave info (top-right) ---
            {
                const auto waveText = TextFormat("WAVE %d", wave);
                const Vector2 ext = measure(waveText, 28);
                drawLabel({x0 + vp.width - ext.x - 24, y0 + 18}, waveText, 28, pal::hudText);
                const auto hostileText = TextFormat("HOSTILES %d", hostiles);
                const Vector2 ext2 = measure(hostileText, 16);
                drawLabel({x0 + vp.width - ext2.x - 24, y0 + 52}, hostileText, 16, pal::hudDim);
            }

            // --- Hearts + shield (bottom-left) ---
            const float heartsY = y0 + vp.height - 64;
            for (int i = 0; i < cfg.playerMaxHp; ++i) {
                const Vector2 at{x0 + 34 + static_cast<float>(i) * 30.0f, heartsY};
                const bool full = i < hp;
                if (full) {
                    DrawPoly(at, 4, 10.0f, 45.0f, pal::player);
                }
                DrawPolyLinesEx(at, 4, 10.0f, 45.0f, 2.0f, full ? pal::player : pal::hudDim);
            }
            if (shield) {
                const Vector2 at{x0 + 34 + static_cast<float>(cfg.playerMaxHp) * 30.0f + 6.0f, heartsY};
                DrawRing(at, 7.0f, 10.0f, 0.0f, 360.0f, 24, pal::pickupShield);
            }

            // --- Dash cooldown (bottom-left, below hearts) ---
            {
                const float barY = heartsY + 22;
                drawLabel({x0 + 24, barY - 2}, "DASH", 12, pal::hudDim);
                const float barX = x0 + 70;
                DrawRectangle(static_cast<int>(barX), static_cast<int>(barY),
                              static_cast<int>(90.0f * dashFrac), 6,
                              dashFrac >= 1.0f
                                  ? Fade(pal::player, 0.7f + 0.3f * std::sin(time * 8.0f))
                                  : Fade(pal::hudDim, 0.8f));
                DrawRectangleLines(static_cast<int>(barX), static_cast<int>(barY), 90, 6,
                                   Fade(pal::hudDim, 0.5f));
            }

            // --- Power-up timers (bottom-right) ---
            {
                float py = y0 + vp.height - 42;
                const float px = x0 + vp.width - 190;
                if (rapidLeft > 0.0f) {
                    drawLabel({px, py}, "RAPID", 14, pal::pickupRapid);
                    DrawRectangle(static_cast<int>(px + 70), static_cast<int>(py + 4),
                                  static_cast<int>(100.0f * rapidLeft / cfg.powerUpDuration), 6,
                                  Fade(pal::pickupRapid, 0.85f));
                    py -= 22;
                }
                if (tripleLeft > 0.0f) {
                    drawLabel({px, py}, "TRIPLE", 14, pal::pickupTriple);
                    DrawRectangle(static_cast<int>(px + 70), static_cast<int>(py + 4),
                                  static_cast<int>(100.0f * tripleLeft / cfg.powerUpDuration), 6,
                                  Fade(pal::pickupTriple, 0.85f));
                }
            }

            // --- Center banner ---
            if (bannerTimer > 0.0f) {
                const float progress = 1.0f - bannerTimer / bannerDuration;
                float alpha = 1.0f;
                if (progress < 0.15f)
                    alpha = progress / 0.15f;
                else if (progress > 0.75f)
                    alpha = (1.0f - progress) / 0.25f;
                const float size = 54.0f * (1.0f + 0.1f * (1.0f - std::min(1.0f, progress / 0.15f)));
                const Vector2 ext = measure(bannerText.c_str(), size);
                const Vector2 at{x0 + vp.width * 0.5f - ext.x * 0.5f, y0 + vp.height * 0.32f};
                drawLabel({at.x + 2, at.y + 2}, bannerText.c_str(), size, Fade(BLACK, alpha * 0.6f));
                drawLabel(at, bannerText.c_str(), size, Fade(bannerColor, alpha));
            }

            // --- Pause overlay ---
            if (paused) {
                DrawRectangleRec(vp, Fade(BLACK, 0.55f));
                const Vector2 ext = measure("PAUSED", 64);
                drawLabel({x0 + vp.width * 0.5f - ext.x * 0.5f, y0 + vp.height * 0.42f}, "PAUSED", 64,
                          pal::hudText);
                const Vector2 ext2 = measure("P - RESUME    ESC - QUIT TO MENU", 18);
                drawLabel({x0 + vp.width * 0.5f - ext2.x * 0.5f, y0 + vp.height * 0.42f + 80},
                          "P - RESUME    ESC - QUIT TO MENU", 18, pal::hudDim);
            }
        });
    }

} // namespace neon
