#pragma once
#include <unordered_map>
#include <string>

#include "raylib.h"

namespace breakout {

// All possible power-up effects
enum class PowerUpType {
    // Paddle modifiers
    WidePaddle,
    NarrowPaddle,
    LaserPaddle,
    StickyPaddle,

    // Ball modifiers
    MultiBall,
    FireBall,
    SlowBall,
    FastBall,

    // Game modifiers
    ExtraLife,
    ScoreMultiplier,

    // Field modifiers
    SafetyNet,
};

struct PowerUpConfig {
    PowerUpType type;
    float duration{0.0f};       // 0 = instant/permanent, >0 = timed effect
    float magnitude{1.0f};      // Effect strength (e.g., 1.5x paddle width)
    Color color{PURPLE};        // Visual color for the falling power-up
    std::string displayName;    // For HUD/FX
};

// Registry of all power-up configurations
inline const std::unordered_map<PowerUpType, PowerUpConfig> kPowerUpConfigs = {
    {PowerUpType::WidePaddle,      {PowerUpType::WidePaddle, 10.0f, 1.5f, BLUE, "Wide Paddle"}},
    {PowerUpType::NarrowPaddle,    {PowerUpType::NarrowPaddle, 8.0f, 0.6f, RED, "Narrow Paddle"}},
    {PowerUpType::LaserPaddle,     {PowerUpType::LaserPaddle, 12.0f, 1.0f, ORANGE, "Laser"}},
    {PowerUpType::StickyPaddle,    {PowerUpType::StickyPaddle, 15.0f, 1.0f, PINK, "Sticky"}},
    {PowerUpType::MultiBall,       {PowerUpType::MultiBall, 0.0f, 3.0f, GREEN, "Multi-Ball"}},
    {PowerUpType::FireBall,        {PowerUpType::FireBall, 8.0f, 1.0f, ORANGE, "Fire Ball"}},
    {PowerUpType::SlowBall,        {PowerUpType::SlowBall, 10.0f, 0.7f, SKYBLUE, "Slow Ball"}},
    {PowerUpType::FastBall,        {PowerUpType::FastBall, 8.0f, 1.4f, RED, "Fast Ball"}},
    {PowerUpType::ExtraLife,       {PowerUpType::ExtraLife, 0.0f, 1.0f, GREEN, "+1 Life"}},
    {PowerUpType::ScoreMultiplier, {PowerUpType::ScoreMultiplier, 15.0f, 2.0f, GOLD, "2x Score"}},
    {PowerUpType::SafetyNet,       {PowerUpType::SafetyNet, 20.0f, 1.0f, LIME, "Safety Net"}},
};

inline const char* iconForPowerUp(PowerUpType type) {
    switch (type) {
        case PowerUpType::WidePaddle: return "W";
        case PowerUpType::NarrowPaddle: return "N";
        case PowerUpType::LaserPaddle: return "L";
        case PowerUpType::StickyPaddle: return "S";
        case PowerUpType::MultiBall: return "M";
        case PowerUpType::FireBall: return "F";
        case PowerUpType::SlowBall: return "-";
        case PowerUpType::FastBall: return "+";
        case PowerUpType::ExtraLife: return "H";
        case PowerUpType::ScoreMultiplier: return "2";
        case PowerUpType::SafetyNet: return "=";
        default: return "?";
    }
}

} // namespace breakout
