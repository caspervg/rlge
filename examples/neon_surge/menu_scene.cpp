#include "menu_scene.hpp"

#include <cmath>

#include "runtime.hpp"

#include "backdrop.hpp"
#include "ns_config.hpp"
#include "ns_game.hpp"

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

    MenuScene::MenuScene(Runtime& r, NsGame* game) :
        Scene(r), game_(game) {}

    void MenuScene::enter() {
        camera_.setTarget({cfg.screenWidth * 0.5f, cfg.screenHeight * 0.5f});
        setSingleView(camera_);

        spawn<NebulaBackdrop>(game_, Rectangle{-1200.0f, -900.0f, cfg.screenWidth + 2400.0f,
                                               cfg.screenHeight + 1800.0f});
        spawn<Starfield>(110);

        game_->assets().sfx.play("pad", 0.6f);
        timers().every(4.1f, [this] { game_->assets().sfx.play("pad", 0.6f); });
    }

    void MenuScene::update(const float dt) {
        Scene::update(dt);
        time_ += dt;

        // Slow orbital drift makes the starfield parallax visible.
        camera_.setTarget({
            cfg.screenWidth * 0.5f + std::sin(time_ * 0.11f) * 130.0f,
            cfg.screenHeight * 0.5f + std::cos(time_ * 0.14f) * 90.0f
        });
        fitCamera_();

        if (input().pressed(Action::Confirm) || input().keyPressed(KeyCode::Space)) {
            gameEvents().enqueue(StartGameRequested{});
        }
        if (input().keyPressed(KeyCode::Escape)) {
            runtime().quit();
        }
    }

    void MenuScene::fitCamera_() {
        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;
        camera_.setOffset({vp.x + vp.width * 0.5f, vp.y + vp.height * 0.5f});
        camera_.setZoom(vp.height > 0.0f ? vp.height / cfg.screenHeight : 1.0f);
    }

    void MenuScene::draw() {
        Scene::draw();

        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;
        const float time = time_;
        const long long best = game_->highScore();
        const Texture2D glowTex = game_->assets().glow;

        rq().submitUI([vp, time, best, glowTex] {
            const float cx = vp.x + vp.width * 0.5f;

            // --- Title with layered glow ---
            const char* title = "NEON SURGE";
            const float titleSize = 92.0f;
            const Vector2 ext = measure(title, titleSize);
            const Vector2 at{cx - ext.x * 0.5f, vp.y + vp.height * 0.2f};
            const float pulse = 0.75f + 0.25f * std::sin(time * 2.1f);

            DrawTexturePro(glowTex,
                           Rectangle{0, 0, static_cast<float>(glowTex.width), static_cast<float>(glowTex.height)},
                           Rectangle{cx, at.y + ext.y * 0.5f, ext.x * 1.8f, ext.y * 5.0f},
                           Vector2{ext.x * 0.9f, ext.y * 2.5f}, 0.0f, Fade(pal::player, 0.16f * pulse));

            for (int i = 1; i <= 3; ++i) {
                const auto offset = static_cast<float>(i) * 1.5f;
                drawLabel({at.x - offset, at.y}, title, titleSize, Fade(pal::player, 0.10f * pulse));
                drawLabel({at.x + offset, at.y}, title, titleSize, Fade(pal::chaser, 0.10f * pulse));
            }
            drawLabel(at, title, titleSize, Fade(WHITE, 0.85f + 0.15f * pulse));

            const char* subtitle = "AN RLGE ARENA SHOOTER";
            const Vector2 subExt = measure(subtitle, 18);
            drawLabel({cx - subExt.x * 0.5f, at.y + ext.y + 10.0f}, subtitle, 18, pal::hudDim);

            // --- Enemy legend ---
            const float legendY = vp.y + vp.height * 0.52f;
            struct LegendEntry {
                const char* name;
                Color color;
                int sides;
                float radius;
            };
            const LegendEntry entries[4] = {
                {"CHASER", pal::chaser, 3, 12.0f},
                {"WEAVER", pal::weaver, 4, 11.0f},
                {"SPLITTER", pal::splitter, 6, 15.0f},
                {"COMET", pal::comet, 4, 10.0f},
            };
            const float spacing = 150.0f;
            float lx = cx - spacing * 1.5f;
            for (const auto& [name, color, sides, radius] : entries) {
                const Vector2 iconAt{lx, legendY};
                const float spin = time * 40.0f;
                DrawPolyLinesEx(iconAt, sides, radius, spin, 2.0f, color);
                const Vector2 nameExt = measure(name, 14);
                drawLabel({lx - nameExt.x * 0.5f, legendY + 24.0f}, name, 14, pal::hudDim);
                lx += spacing;
            }

            // --- Prompt ---
            if (std::fmod(time, 1.0f) < 0.65f) {
                const char* prompt = "PRESS ENTER TO ENGAGE";
                const Vector2 promptExt = measure(prompt, 26);
                drawLabel({cx - promptExt.x * 0.5f, vp.y + vp.height * 0.68f}, prompt, 26, pal::player);
            }

            // --- Best score ---
            if (best > 0) {
                const auto text = TextFormat("BEST %lld", best);
                const Vector2 bestExt = measure(text, 20);
                drawLabel({cx - bestExt.x * 0.5f, vp.y + vp.height * 0.76f}, text, 20, pal::comet);
            }

            // --- Controls ---
            const char* lines[3] = {
                "WASD / LEFT STICK - MOVE      MOUSE / RIGHT STICK - AIM",
                "LMB / SPACE / RT - FIRE       SHIFT / RMB / A - DASH",
                "P - PAUSE      F11 - FULLSCREEN      F12 - DEBUG",
            };
            float ly = vp.y + vp.height * 0.85f;
            for (const auto* line : lines) {
                const Vector2 lineExt = measure(line, 14);
                drawLabel({cx - lineExt.x * 0.5f, ly}, line, 14, pal::hudDim);
                ly += 20.0f;
            }
        });
    }

} // namespace neon
