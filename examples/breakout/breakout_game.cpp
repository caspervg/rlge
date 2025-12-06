#include "breakout_game.hpp"

#include "breakout_scene.hpp"
#include "game_over_scene.h"
#include "runtime.hpp"

namespace breakout {

    BreakoutGame::BreakoutGame(Runtime& runtime, const std::string_view levelFilePath) :
        runtime_(runtime), levelManager_(levelFilePath.data()) {
        loadHighScore_();
        subscribeToEvents_();

        runtime_.onResize([this](const float width, const float height) {
            // TODO: Handle resizing more nicely.
            g_cfg.viewPortHeight = height;
            g_cfg.viewPortWidth = width;
            restart();
        });
    }

    BreakoutGame::~BreakoutGame() { unsubscribeFromEvents_(); }d

    void BreakoutGame::start() {
        state_ = {
            .levelName = levelManager_.currentLevel()->name,
            .levelIndex = levelManager_.currentLevelIndex(),
            .numLevels = levelManager_.numLevels(),
            .totalScore = 0,
            .livesRemaining = levelManager_.currentLevel()->lives,
            .isGameOver = false,
            .highScore = state_.highScore
        };

        runtime_.pushScene<BreakoutScene>(this);
    }

    void BreakoutGame::restart() {
        runtime_.clearScenes();
        levelManager_.reset();
        start();
    }

    void BreakoutGame::completeLevel(const int levelScore) {
        incrementScore(levelScore);

        if (levelManager_.nextLevel()) {
            // A next level exists and the level manager has now switched to it
            transitionToLevel_();
        } else {
            // Game won!
            gameOver();
        }
    }

    void BreakoutGame::loseLife() {
        state_.livesRemaining--;
        if (state_.livesRemaining == 0) {
            gameOver();
        }
    }

    void BreakoutGame::gainLife() {
        state_.livesRemaining++;
    }

    void BreakoutGame::gameOver() {
        state_.isGameOver = true;
        saveHighScore_();
        transitionToGameOver_();
    }

    void BreakoutGame::incrementScore(const int points) { state_.totalScore += points; }

    const Level* BreakoutGame::currentLevel() const { return levelManager_.getLevel(state_.levelIndex); }

    bool BreakoutGame::hasNextLevel() const { return state_.levelIndex + 1 < levelManager_.numLevels(); }

    void BreakoutGame::subscribeToEvents_() {
        auto& bus = runtime_.services().gameEvents();

        levelCompletedId_ = bus.subscribe<LevelCompleted>([this](const LevelCompleted& e) {
            completeLevel(e.levelScore);
        });

        restartGameId_ = bus.subscribe<RestartGame>([this](const RestartGame& _) { restart(); });
    }

    void BreakoutGame::unsubscribeFromEvents_() const {
        auto& bus = runtime_.services().gameEvents();

        bus.unsubscribe<LevelCompleted>(levelCompletedId_);
        bus.unsubscribe<RestartGame>(restartGameId_);
    }

    void BreakoutGame::transitionToLevel_() {
        runtime_.transitionTo<BreakoutScene>(std::make_unique<FadeTransition>(0.35f), this);
        state_.levelIndex = levelManager_.currentLevelIndex();
        state_.livesRemaining += levelManager_.currentLevel()->lives;
        state_.levelName = levelManager_.currentLevel()->name;
    }

    void BreakoutGame::transitionToGameOver_() {
        runtime_.transitionTo<GameOverScene>(std::make_unique<FadeTransition>(0.3f), state_.totalScore);
    }

    void BreakoutGame::loadHighScore_() {
        // TODO
    }

    void BreakoutGame::saveHighScore_() {
        // TODO
    }

} // namespace breakout
