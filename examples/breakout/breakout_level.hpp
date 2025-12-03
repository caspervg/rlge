#pragma once
#include <memory>
#include <string>
#include <vector>

#include "breakout_config.hpp"
#include "powerup_types.hpp"
#include "raylib.h"

namespace cpptoml {
    class table_array;
    class table;
}

namespace breakout {
    enum BrickType {
        Standard
    };

    struct BrickConfig {
        int x{0};
        int y{0};
        int hitPoints{1};
        Color color{LIME};
        BrickType type{Standard};
        std::vector<PowerUpType> containedPowerUps{}; // Multiple power-ups possible
        float powerUpDropChance{0.0f};                // Random drop if none explicitly listed
    };

    struct Level {
        float ballRadius = 5.0f;
        float ballVelocityMaximum = 900.0f;
        float ballVelocityMultiplier = 1.025f;
        Vector2 ballVelocityStart = {0.0f, -350.0f};

        std::vector<BrickConfig> bricks = {};
        int brickRows = 5;
        int brickColumns = 10;

        int lives = 3;
        std::string name = "Unnamed Level";
        float paddleSpeed = 400.0f;
        float paddleWidth = 80.0f;
    };

    class LevelManager {
    public:
        LevelManager();
        explicit LevelManager(std::string_view levelFile);

        [[nodiscard]] const Level* currentLevel() const;
        [[nodiscard]] size_t currentLevelIndex() const;
        [[nodiscard]] const Level* getLevel(size_t index) const;
        [[nodiscard]] bool nextLevel();
        [[nodiscard]] size_t numLevels() const;
        void reset();

    private:
        BrickType brickTypeFromString_(const std::string& str);
        std::vector<BrickConfig> generateBricks_(int rows, int columns);
        std::vector<BrickConfig> parseBricks_(int rows, int columns, const std::shared_ptr<cpptoml::table_array>& bricksArr);

    private:
        size_t currentLevelIndex_{0};
        std::vector<std::unique_ptr<Level>> levels_;
    };

} // namespace breakout
