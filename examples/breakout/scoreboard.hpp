#pragma once
#include "breakout_game.hpp"
#include "render_entity.hpp"

namespace breakout {

class ScoreBoard final : public RenderEntity {
public:
    explicit ScoreBoard(Scene& s, const BreakoutGame& game);
    void draw() override;

private:
    const BreakoutGame& game_;
};

} // namespace breakout
