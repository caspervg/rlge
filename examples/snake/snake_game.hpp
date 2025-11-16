#pragma once

#include <random>
#include <vector>

#include "raylib.h"

namespace rlge {
    class EventBus;
}

namespace snake {

    inline constexpr int kTilesX = 20;
    inline constexpr int kTilesY = 15;
    inline constexpr int kPixelsPerTile = 8;
    inline constexpr int kMagnification = 4;
    inline constexpr int kTilePixels = kPixelsPerTile * kMagnification;
    inline constexpr int kScreenPixelsX = kTilesX * kTilePixels;
    inline constexpr int kScreenPixelsY = kTilesY * kTilePixels;

    struct Config {
        int tilesX = kTilesX;
        int tilesY = kTilesY;
        int pixelsPerTile = kPixelsPerTile;
        int magnification = kMagnification;
        float movesPerSecond = 5.0f; // tiles per second
    };

    enum class Direction { Left, Right, Up, Down };

    struct AppleEaten {
        int amount = 1;
    };

    struct SnakeDied {};

    struct RestartGame {};

    class Game {
    public:
        struct Cell {
            int x;
            int y;
        };

        Game(const Config& cfg, rlge::EventBus* bus = nullptr);

        void setDirection(Direction dir);
        Direction direction() const { return dir_; }

        void update(float dt);

        Vector2 headWorldPos() const;
        Vector2 appleWorldPos() const;

        const std::vector<Cell>& body() const { return body_; }
        Vector2 worldPos(const Cell& c) const { return tileCenter(c.x, c.y); }

        [[nodiscard]] std::default_random_engine* rng() { return &rng_; }

        int score() const { return score_; }
        const Config& config() const { return cfg_; }

    private:
        Config cfg_;
        int tilePixels_;
        int screenWidth_;
        int screenHeight_;

        std::vector<Cell> body_;
        int appleX_;
        int appleY_;

        Direction dir_;
        float moveInterval_;
        float moveAccum_ = 0.0f;

        int score_ = 0;

        std::default_random_engine rng_;
        std::uniform_int_distribution<> appleXRng_;
        std::uniform_int_distribution<> appleYRng_;
        rlge::EventBus* bus_;

        void step();
        void spawnApple();
        Vector2 tileCenter(int gx, int gy) const;
    };

} // namespace snake
