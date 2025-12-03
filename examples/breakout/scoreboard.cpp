#include "scoreboard.hpp"

#include <format>
#include "raylib.h"

namespace breakout {
    using namespace rlge;

    ScoreBoard::ScoreBoard(Scene& s, const BreakoutGame& game) : RenderEntity(s), game_(game) {}

    void ScoreBoard::draw() {
        rq().submitUI([this] {
            const auto line1 = std::format("L: {}/{}", game_.displayLevelNumber(), game_.numLevels());
            const auto line2 = std::format("S: {}", game_.displayTotalScore());
            const auto line3 = std::format("H: {}", game_.displayLivesRemaining());
            DrawText(line1.c_str(), 10, 10, 20, WHITE);
            DrawText(line2.c_str(), 10, 30, 20, GREEN);
            DrawText(line3.c_str(), 10, 50, 20, RED);
        });
    }
} // namespace breakout
