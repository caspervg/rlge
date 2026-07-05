#include "game_over_scene.hpp"

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

    GameOverScene::GameOverScene(Runtime& r, NsGame* game) :
        Scene(r), game_(game) {}

    void GameOverScene::enter() {
        camera_.setTarget({cfg.screenWidth * 0.5f, cfg.screenHeight * 0.5f});
        setSingleView(camera_);

        auto& backdrop = spawn<NebulaBackdrop>(game_, Rectangle{-1200.0f, -900.0f, cfg.screenWidth + 2400.0f,
                                                                cfg.screenHeight + 1800.0f});
        backdrop.setDanger(0.55f);
        spawn<Starfield>(90);

        game_->assets().sfx.play("pad", 0.5f, 0.8f);
        timers().every(5.2f, [this] { game_->assets().sfx.play("pad", 0.5f, 0.8f); });
    }

    void GameOverScene::update(const float dt) {
        Scene::update(dt);
        time_ += dt;

        camera_.setTarget({
            cfg.screenWidth * 0.5f + std::sin(time_ * 0.08f) * 60.0f,
            cfg.screenHeight * 0.5f + std::cos(time_ * 0.1f) * 40.0f
        });
        fitCamera_();

        if (input().pressed(Action::Confirm) || input().keyPressed(KeyCode::Space)) {
            gameEvents().enqueue(RestartRequested{});
        }
        if (input().keyPressed(KeyCode::M) || input().keyPressed(KeyCode::Escape)) {
            gameEvents().enqueue(BackToMenuRequested{});
        }
    }

    void GameOverScene::fitCamera_() {
        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;
        camera_.setOffset({vp.x + vp.width * 0.5f, vp.y + vp.height * 0.5f});
        camera_.setZoom(vp.height > 0.0f ? vp.height / cfg.screenHeight : 1.0f);
    }

    void GameOverScene::draw() {
        Scene::draw();

        const View* view = primaryView();
        if (!view)
            return;
        const Rectangle vp = view->viewport;
        const float time = time_;
        const GameOverStats stats = game_->lastRun();
        const long long best = game_->highScore();

        rq().submitUI([vp, time, stats, best] {
            const float cx = vp.x + vp.width * 0.5f;

            const char* title = "SHIP DESTROYED";
            const float titleSize = 68.0f;
            const Vector2 ext = measure(title, titleSize);
            const Vector2 at{cx - ext.x * 0.5f, vp.y + vp.height * 0.18f};
            const float pulse = 0.8f + 0.2f * std::sin(time * 3.0f);
            drawLabel({at.x + 3, at.y + 3}, title, titleSize, Fade(BLACK, 0.6f));
            drawLabel(at, title, titleSize, Fade(pal::chaser, pulse));

            // --- Stats ---
            float y = vp.y + vp.height * 0.4f;
            const auto statLine = [&](const char* label, const char* value, const Color valueColor) {
                const Vector2 labelExt = measure(label, 20);
                drawLabel({cx - 190.0f, y}, label, 20, pal::hudDim);
                (void)labelExt;
                const Vector2 valueExt = measure(value, 26);
                drawLabel({cx + 190.0f - valueExt.x, y - 3.0f}, value, 26, valueColor);
                y += 42.0f;
            };

            statLine("FINAL SCORE", TextFormat("%lld", stats.score), pal::hudText);
            statLine("WAVE REACHED", TextFormat("%d", stats.wave), pal::player);
            statLine("HOSTILES DOWN", TextFormat("%d", stats.kills), pal::comet);
            statLine("BEST", TextFormat("%lld", best), pal::pickupGem);

            if (stats.newHighScore && std::fmod(time, 0.8f) < 0.55f) {
                const char* banner = "NEW HIGH SCORE";
                const Vector2 bannerExt = measure(banner, 34);
                drawLabel({cx - bannerExt.x * 0.5f, y + 14.0f}, banner, 34, pal::comet);
            }

            // --- Prompt ---
            if (std::fmod(time, 1.0f) < 0.65f) {
                const char* prompt = "ENTER - RETRY      M - MENU";
                const Vector2 promptExt = measure(prompt, 24);
                drawLabel({cx - promptExt.x * 0.5f, vp.y + vp.height * 0.78f}, prompt, 24, pal::player);
            }
        });
    }

} // namespace neon
