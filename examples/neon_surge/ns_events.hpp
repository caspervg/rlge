#pragma once
#include "raylib.h"

namespace neon {

    enum class PickupType {
        RapidFire,
        TripleShot,
        Shield,
        Heal,
        ScoreGem
    };

    enum class EnemyKind {
        Chaser,
        Weaver,
        Splitter,
        Shard,
        Comet
    };

    // ---- Scene-local events (arena) ----
    struct EnemyKilled {
        EnemyKind kind;
        Vector2 pos;
        int baseScore;
    };

    struct PlayerDamaged {
        int hpLeft;
        Vector2 pos;
    };

    struct PlayerDied {
        Vector2 pos;
    };

    struct WaveStarted {
        int wave;
    };

    struct WaveCleared {
        int wave;
    };

    struct PowerUpCollected {
        PickupType type;
        Vector2 pos;
    };

    struct ScoreChanged {
        long long total;
        int multiplier;
    };

    // ---- Game-wide events ----
    struct StartGameRequested {};
    struct RestartRequested {};
    struct BackToMenuRequested {};

    struct GameOverStats {
        long long score = 0;
        int wave = 1;
        int kills = 0;
        bool newHighScore = false;
    };

} // namespace neon
