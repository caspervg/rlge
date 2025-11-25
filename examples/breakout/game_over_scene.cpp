#include "game_over_scene.h"
#include "breakout_config.hpp"

#include <format>

#include "breakout_game.hpp"

namespace breakout {
    void Overlay::draw() {
        rq().submitUI([this] {
            const auto gameOverText = std::format("Game over! Score: {}", score_);
            const auto gameOverTextWidth = MeasureText(gameOverText.c_str(), 30);
            const auto gameOverTextPosX = g_cfg.width / 2 - gameOverTextWidth / 2;
            const auto gameOverTextPosY = g_cfg.height / 2 - 15;
            DrawRectangle(gameOverTextPosX - 5, gameOverTextPosY, gameOverTextWidth + 10, 30, Fade(BLACK, 0.75f));
            DrawText(gameOverText.c_str(), gameOverTextPosX, gameOverTextPosY, 30, WHITE);

            const auto restartText = std::format("Press [{}] to restart", "ENTER");
            const auto restartTextWidth = MeasureText(restartText.c_str(), 20);
            const auto restartTextPosX = g_cfg.width / 2 - restartTextWidth / 2;
            const auto restartTextPosY = g_cfg.height / 2 + 15;
            DrawRectangle(restartTextPosX - 5, restartTextPosY, restartTextWidth + 10, 30, Fade(BLACK, 0.75f));
            DrawText(restartText.c_str(), restartTextPosX, restartTextPosY, 20, WHITE);
        });
    }

    GameOverScene::~GameOverScene() = default;

    void GameOverScene::enter() {
        overlay_ = &spawn<Overlay>(score_);
    }

    void GameOverScene::exit() { overlay_ = nullptr; }

    void GameOverScene::update(float dt) {
        const auto& in = input();
        if (in.pressed(Action::Confirm)) {
            gameEvents().enqueue(RestartGame{});
        }
    }

}
