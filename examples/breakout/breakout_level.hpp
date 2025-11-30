#pragma once
#include <memory>
#include <string>
#include <unordered_set>

#include "breakout_config.hpp"
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

        bool operator==(const BrickConfig& other) const {
            return x == other.x
                && y == other.y
                && hitPoints == other.hitPoints
                && color.r == other.color.r
                && color.g == other.color.g
                && color.b == other.color.b
                && color.a == other.color.a
                && type == other.type;
        }
    };
}

template <>
struct std::hash<breakout::BrickConfig> {
    size_t operator()(const breakout::BrickConfig& b) const noexcept {
        size_t h = 0;
        auto combine = [&h](const size_t v) {
            h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        };
        combine(std::hash<int>{}(b.x));
        combine(std::hash<int>{}(b.y));
        combine(std::hash<int>{}(b.hitPoints));
        combine(std::hash<unsigned char>{}(b.color.r));
        combine(std::hash<unsigned char>{}(b.color.g));
        combine(std::hash<unsigned char>{}(b.color.b));
        combine(std::hash<unsigned char>{}(b.color.a));
        combine(std::hash<int>{}(b.type));
        return h;
    }
};

namespace breakout {
    struct Level {
        float ballRadius = 5.0f;
        float ballVelocityMaximum = 900.0f;
        float ballVelocityMultiplier = 1.025f;
        Vector2 ballVelocityStart = {0.0f, -350.0f};

        std::unordered_set<BrickConfig> bricks = {};
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
        explicit LevelManager(const std::string& levelFile);

        [[nodiscard]] const Level* currentLevel() const;
        [[nodiscard]] size_t currentLevelIndex() const;
        [[nodiscard]] const Level* getLevel(int index) const;
        [[nodiscard]] bool nextLevel();
        [[nodiscard]] size_t numLevels() const;
        void reset();

    private:
        BrickType brickTypeFromString_(const std::string& str);
        std::unordered_set<BrickConfig> generateBricks_(int rows, int columns);
        std::unordered_set<BrickConfig> parseBricks_(int rows, int columns, const std::shared_ptr<cpptoml::table_array>& bricksArr);

    private:
        size_t currentLevelIndex_{0};
        std::vector<std::unique_ptr<Level>> levels_;
    };

}
