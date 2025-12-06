#include "game_over_scene.h"
#include "breakout_config.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "breakout_events.hpp"
#include "runtime.hpp"
#include "window.hpp"

namespace breakout {
    namespace {
        std::pair<Rectangle, float> uiFrame(const rlge::Scene& scene) {
            Rectangle view{0.0f, 0.0f, static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
            if (const auto* primary = scene.runtime().primaryView()) {
                view = primary->viewport;
            }

            const Vector2 dpi = scene.runtime().window().dpiScale();
            const float dpiScale = std::max(dpi.x, dpi.y);
            const float viewportScale = view.height > 0.0f ? view.height / g_cfg.viewPortHeight : 1.0f;
            return {view, dpiScale * viewportScale};
        }
    }

    void Overlay::draw() {
        rq().submitUI([this] {
            const auto [view, scale] = uiFrame(scene());
            const int titleSize = std::max(20, static_cast<int>(30 * scale));
            const int promptSize = std::max(14, static_cast<int>(20 * scale));
            const int padding = static_cast<int>(6 * scale);

            const auto gameOverText = std::format("Game over! Score: {}", score_);
            const auto gameOverTextWidth = MeasureText(gameOverText.c_str(), titleSize);
            const auto gameOverTextPosX = view.x + view.width / 2.0f - gameOverTextWidth / 2.0f;
            const auto gameOverTextPosY = view.y + view.height / 2.0f - titleSize;
            DrawRectangle(static_cast<int>(gameOverTextPosX) - padding, static_cast<int>(gameOverTextPosY),
                          gameOverTextWidth + padding * 2, titleSize, Fade(BLACK, 0.75f));
            DrawText(gameOverText.c_str(), static_cast<int>(gameOverTextPosX), static_cast<int>(gameOverTextPosY),
                     titleSize, WHITE);

            const auto restartText = std::format("Press [{}] to restart", "ENTER");
            const auto restartTextWidth = MeasureText(restartText.c_str(), promptSize);
            const auto restartTextPosX = view.x + view.width / 2.0f - restartTextWidth / 2.0f;
            const auto restartTextPosY = gameOverTextPosY + titleSize + padding * 2;
            DrawRectangle(static_cast<int>(restartTextPosX) - padding, static_cast<int>(restartTextPosY),
                          restartTextWidth + padding * 2, promptSize + padding, Fade(BLACK, 0.75f));
            DrawText(restartText.c_str(), static_cast<int>(restartTextPosX), static_cast<int>(restartTextPosY),
                     promptSize, WHITE);
        });
    }

    GameOverScene::~GameOverScene() = default;

    void GameOverScene::enter() {
        overlay_ = &spawn<Overlay>(score_);
    }

    void GameOverScene::exit() { overlay_ = nullptr; }

    void GameOverScene::update(const float dt) {
        Scene::update(dt);

        const auto& in = input();
        if (in.pressed(Action::Confirm)) {
            gameEvents().enqueue(RestartGame{});
        }
    }

}
