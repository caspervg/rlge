#include "breakout_game.hpp"

#include "breakout_scene.hpp"
#include "game_over_scene.h"
#include "runtime.hpp"

namespace breakout {

    BreakoutGame::BreakoutGame(Runtime& runtime, const std::string_view levelFilePath) :
        runtime_(runtime), levelManager_(levelFilePath.data()) {
        loadHighScore_();
        subscribeToEvents_();
    }

    BreakoutGame::~BreakoutGame() { unsubscribeFromEvents_(); }

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
        start();
    }

    void BreakoutGame::completeLevel(const int levelScore) {
        incrementScore(levelScore);

        if (hasNextLevel()) {
            state_.levelIndex++;
            transitionToLevel_(state_.levelIndex);
        }
        else {
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

        levelCompletedId_ = bus.subscribe<LevelCompleted>([this](const LevelCompleted& e) { completeLevel(e.levelScore); });

        ballLostId_ = bus.subscribe<BallLost>([this](const BallLost& e) { loseLife(); });

        restartGameId_ = bus.subscribe<RestartGame>([this](const RestartGame& _) { restart(); });
    }

    void BreakoutGame::unsubscribeFromEvents_() const {
        auto& bus = runtime_.services().gameEvents();

        bus.unsubscribe<LevelCompleted>(levelCompletedId_);
        bus.unsubscribe<BallLost>(ballLostId_);
        bus.unsubscribe<RestartGame>(restartGameId_);
    }

    void BreakoutGame::transitionToLevel_(const size_t levelIndex) {
        runtime_.transitionTo<BreakoutScene>(std::make_unique<FadeTransition>(0.35f), this);
        state_.livesRemaining += levelManager_.getLevel(levelIndex)->lives;
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
