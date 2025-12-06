#pragma once
#include <string>
#include <vector>

#include "raylib.h"

namespace breakout {
    struct Level;

    struct Config {
        // Virtual playfield size; actual window can be any size and will letterbox to this aspect.
        float viewPortWidth = 960.0f;
        float viewPortHeight = 540.0f;
        int brickWidth = 30;
        int brickHeight = 15;
        int brickMargin = 5;
        float ballRadius = 5.0f;
        int paddleWidth = 80;
        int paddleHeight = 15;
        float maxBallPaddleDeflectionAngle = 60.0f; // degrees
        Color paddleColor = BLUE;
        Color brickColor = LIME;
        Color ballColor = YELLOW;
        float wallThickness = 20.0f;
        float brickHitShakeIntensity = 0.25f;
        float brickHitShakeDuration = 0.1f;
    };

    inline Config g_cfg;

}
