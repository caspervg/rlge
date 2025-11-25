#pragma once

namespace breakout {

    struct GameWon {};
    struct BrickDestroyed {
        int points = 0;
    };
    struct BallLost {};

}