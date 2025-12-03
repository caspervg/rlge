#pragma once
#include <optional>
#include <string_view>
#include "breakout_events.hpp"
#include "breakout_level.hpp"
#include "events.hpp"

namespace rlge {
    class Runtime;
    class Scene;
}

namespace breakout {
    using namespace rlge;

    class BreakoutGame {
    public:

        struct SessionState {
            std::string_view levelName{"Unnamed Level"};
            size_t levelIndex{0};
            size_t numLevels{0};
            int totalScore{0};
            int livesRemaining{3};

            bool isGameOver{false};
            std::optional<int> highScore{std::nullopt};
        };

        explicit BreakoutGame(Runtime& runtime, std::string_view levelFilePath);
        ~BreakoutGame();

        void start();           // Start a new game from level 1
        void restart();         // Restart from level 1 and clear all state

        void completeLevel(int score);   // Called when all bricks are destroyed
        void loseLife();        // Called when the ball is lost
        void gainLife();        // Called when a life is granted
        void gameOver();        // Transition to game over screen

        [[nodiscard]] const SessionState& state() const { return state_; }
        void incrementScore(int points);
        [[nodiscard]] const Level* currentLevel() const;
        [[nodiscard]] size_t numLevels() const { return levelManager_.numLevels(); }
        [[nodiscard]] bool hasNextLevel() const;

        [[nodiscard]] int displayLevelNumber() const { return static_cast<int>(state_.levelIndex) + 1; }
        [[nodiscard]] int displayTotalScore() const { return state_.totalScore; }
        [[nodiscard]] int displayLivesRemaining() const { return state_.livesRemaining; }

    private:
        void subscribeToEvents_();
        void unsubscribeFromEvents_() const;
        void transitionToLevel_(size_t levelIndex);
        void transitionToGameOver_();
        void loadHighScore_();
        void saveHighScore_();

    private:
        Runtime& runtime_;
        LevelManager levelManager_;
        SessionState state_;

        EventBus::SubscriptionId levelCompletedId_{0};
        EventBus::SubscriptionId restartGameId_{0};
    };
}
