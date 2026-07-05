#pragma once
#include <memory>

#include "events.hpp"
#include "render_layer.hpp"

#include "ns_assets.hpp"
#include "ns_events.hpp"

namespace rlge {
    class Runtime;
}

namespace neon {

    // Uniforms for the animated nebula background layer shader.
    struct NebulaParams {
        float time = 0.0f;
        float danger = 0.0f; // 0..1, tints the nebula red when the player is hurting
    };

    // Game orchestrator: owns shared assets/layers, routes scene transitions,
    // and keeps score state that outlives individual scenes.
    class NsGame {
    public:
        explicit NsGame(rlge::Runtime& runtime);
        ~NsGame();

        NsGame(const NsGame&) = delete;
        NsGame& operator=(const NsGame&) = delete;

        void start();

        rlge::Runtime& runtime() { return runtime_; }
        NsAssets& assets() { return *assets_; }

        [[nodiscard]] rlge::LayerId nebulaLayer() const { return nebulaLayer_; }
        [[nodiscard]] rlge::LayerId glowLayer() const { return glowLayer_; }

        [[nodiscard]] long long highScore() const { return highScore_; }
        [[nodiscard]] const GameOverStats& lastRun() const { return lastRun_; }

        // Called by the arena when the player dies.
        void finishRun(const GameOverStats& stats);

    private:
        void subscribe_();
        void loadHighScore_();
        void saveHighScore_() const;

        rlge::Runtime& runtime_;
        std::unique_ptr<NsAssets> assets_;
        rlge::LayerId nebulaLayer_{rlge::InvalidLayerId};
        rlge::LayerId glowLayer_{rlge::InvalidLayerId};

        long long highScore_ = 0;
        GameOverStats lastRun_{};

        std::size_t startSubId_ = 0;
        std::size_t restartSubId_ = 0;
        std::size_t menuSubId_ = 0;
        std::size_t gameOverSubId_ = 0;
    };

} // namespace neon
