#include "game_over_scene.h"

#include <format>

#include "snake_game.hpp"
#include "snake_scene.hpp"

namespace snake {
    void Overlay::draw() {
        rq().submitUI([this] {
            const auto gameOverText = std::format("Game over! Score: {}", score_);
            const auto gameOverTextWidth = MeasureText(gameOverText.c_str(), 30);
            const auto gameOverTextPosX = kScreenPixelsX / 2 - gameOverTextWidth / 2;
            const auto gameOverTextPosY = kScreenPixelsY / 2 - 15;
            DrawRectangle(gameOverTextPosX - 5, gameOverTextPosY, gameOverTextWidth + 10, 30, Fade(BLACK, 0.75f));
            DrawText(gameOverText.c_str(), gameOverTextPosX, gameOverTextPosY, 30, WHITE);

            const auto restartText = std::format("Press [{}] to restart", "ENTER");
            const auto restartTextWidth = MeasureText(restartText.c_str(), 20);
            const auto restartTextPosX = kScreenPixelsX / 2 - restartTextWidth / 2;
            const auto restartTextPosY = kScreenPixelsY / 2 + 15;
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
        const auto& input = engine().input();
        if (input.pressed("enter")) {
            engine().services().events().enqueue(RestartGame{});
        }
    }

}
