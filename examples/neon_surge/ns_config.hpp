#pragma once
#include "raylib.h"

// NEON SURGE - global tuning knobs and palette.
namespace neon {

    struct Config {
        // Virtual/window size
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;

        // Arena (world space, origin at top-left of arena)
        float arenaWidth = 2560.0f;
        float arenaHeight = 1600.0f;
        float gridStep = 160.0f;

        // Player
        float playerRadius = 14.0f;
        float playerAccel = 2800.0f;
        float playerDamping = 5.0f;      // exponential damping per second
        float playerMaxSpeed = 430.0f;
        int playerMaxHp = 4;
        float playerIFrames = 1.4f;      // seconds of invulnerability after a hit

        // Dash
        float dashSpeed = 1150.0f;
        float dashTime = 0.16f;
        float dashCooldown = 1.4f;

        // Weapons
        float fireInterval = 0.15f;
        float fireIntervalRapid = 0.075f;
        float bulletSpeed = 950.0f;
        float bulletRadius = 5.0f;
        float bulletLife = 0.9f;
        float powerUpDuration = 9.0f;

        // Combo
        float comboWindow = 2.6f;
        int comboPerMult = 5;            // kills per +1x multiplier
        int comboMaxMult = 8;

        // Drops
        float dropChance = 0.14f;
        float gemChance = 0.30f;

        // Camera
        float camLerp = 0.10f;
        float camZoom = 1.0f;
    };

    inline const Config cfg{};

    // Neon palette
    namespace pal {
        inline constexpr Color bgDeep{8, 6, 24, 255};
        inline constexpr Color player{64, 255, 238, 255};      // cyan
        inline constexpr Color playerDark{16, 112 , 108, 255};
        inline constexpr Color bullet{255, 244, 120, 255};     // pale yellow
        inline constexpr Color chaser{255, 64, 160, 255};      // magenta
        inline constexpr Color weaver{255, 140, 40, 255};      // orange
        inline constexpr Color splitter{130, 255, 90, 255};    // lime
        inline constexpr Color shard{90, 220, 120, 255};
        inline constexpr Color comet{255, 230, 70, 255};       // gold
        inline constexpr Color pickupRapid{255, 220, 60, 255};
        inline constexpr Color pickupTriple{255, 110, 220, 255};
        inline constexpr Color pickupShield{110, 190, 255, 255};
        inline constexpr Color pickupHeal{120, 255, 140, 255};
        inline constexpr Color pickupGem{190, 130, 255, 255};
        inline constexpr Color grid{60, 60, 130, 70};
        inline constexpr Color border{120, 90, 255, 255};
        inline constexpr Color hudText{210, 215, 255, 255};
        inline constexpr Color hudDim{130, 135, 190, 255};
    }

} // namespace neon
