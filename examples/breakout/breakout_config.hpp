#pragma once
#include "raylib.h"

namespace breakout {
    struct Config {
        int width = 800;
        int height = 600;
        int brickWidth = 30;
        int brickHeight = 15;
        int brickMargin = 5;
        float ballRadius = 5.0f;
        int brickColumns = 20;
        int brickRows = 5;
        int paddleWidth = 80;
        int paddleHeight = 15;
        float paddleSpeed = 400.0f;
        float maxBallPaddleDeflectionAngle = 60.0f; // degrees
        Color paddleColor = BLUE;
        Color brickColor = LIME;
        Color ballColor = YELLOW;
        int initialLives = 3;
        float wallThickness = 20.0f;
        float brickHitShakeIntensity = 0.25f;
        float brickHitShakeDuration = 0.1f;
    };

    inline Config g_cfg;
}