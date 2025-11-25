#pragma once

namespace breakout {

    struct GameLost {
        int finalScore = 0;
    };
    struct GameWon {};
    struct RestartGame {};

    struct BrickDestroyed {
        int points = 0;
    };

    struct BallLost {};

}