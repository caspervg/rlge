#include "scoreboard.hpp"

#include <algorithm>
#include <format>
#include <utility>
#include "raylib.h"
#include "runtime.hpp"
#include "window.hpp"
#include "breakout_config.hpp"

namespace breakout {
    using namespace rlge;

    namespace {
        std::pair<Rectangle, float> uiFrame(const Scene& scene) {
            Rectangle view{0.0f, 0.0f, static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
            if (const auto* primary = scene.runtime().primaryView()) {
                view = primary-> viewport;
            }

            const Vector2 dpi = scene.runtime().window().dpiScale();
            const float dpiScale = std::max(dpi.x, dpi.y);
            const float viewportScale = view.height > 0.0f ? view.height / g_cfg.viewPortHeight : 1.0f;
            return {view, dpiScale * viewportScale};
        }
    }

    ScoreBoard::ScoreBoard(Scene& s, const BreakoutGame& game) : RenderEntity(s), game_(game) {}

    void ScoreBoard::draw() {
        rq().submitUI([this] {
            const auto [view, scale] = uiFrame(scene());
            const int margin = static_cast<int>(10 * scale);
            const int fontSize = std::max(12, static_cast<int>(18 * scale));
            const int lineHeight = fontSize + static_cast<int>(4 * scale);

            const auto line1 = std::format("L: {}/{}", game_.displayLevelNumber(), game_.numLevels());
            const auto line2 = std::format("S: {}", game_.displayTotalScore());
            const auto line3 = std::format("H: {}", game_.displayLivesRemaining());
            const int x = static_cast<int>(view.x) + margin;
            const int y = static_cast<int>(view.y) + margin;
            DrawText(line1.c_str(), x, y, fontSize, WHITE);
            DrawText(line2.c_str(), x, y + lineHeight, fontSize, GREEN);
            DrawText(line3.c_str(), x, y + lineHeight * 2, fontSize, RED);
        });
    }
} // namespace breakout
