#pragma once

namespace breakout {
    struct Config {
        int width = 800;
        int height = 600;
        int brickWidth = 30;
        int brickHeight = 15;
        int brickMargin = 5;
        int ballRadius = 5;
        int brickColumns = 20;
        int brickRows = 5;
        int paddleWidth = 80;
        int paddleHeight = 15;
    };

    struct GameWon {};
    struct BrickDestroyed {
        int points = 0;
    };
    struct BallLost {};

}