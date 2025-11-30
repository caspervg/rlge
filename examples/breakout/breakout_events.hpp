#pragma once
#include "breakout_level.hpp"

namespace breakout {

    struct GameLost {
        int finalScore = 0;
    };

    struct GameWon {
        int finalScore = 0;
    };

    struct LevelCompleted {
        int levelScore = 0;
    };

    struct RestartGame {};

    struct BrickHit {
        BrickConfig brick;
    };

    struct BrickDestroyed {
        int points = 0;
        BrickConfig brick;
    };

    struct BallLost {};

}