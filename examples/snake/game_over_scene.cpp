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
            engine().popScene();
            engine().popScene();
            engine().pushScene<GameScene>();
        }
    }

}
