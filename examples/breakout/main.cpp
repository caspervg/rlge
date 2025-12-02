#include "breakout_level.hpp"
#include "breakout_scene.hpp"
#include "game_over_scene.h"
#include "runtime.hpp"
#include "window.hpp"

using namespace rlge;

int main() {
    const WindowConfig wcfg{
        .width = breakout::g_cfg.viewPortWidth,
        .height = breakout::g_cfg.viewPortHeight,
        .fps = 144,
        .title = "RLGE Breakout"
    };
    Runtime runtime(wcfg);

    // Basic input bindings using type-safe enums
    runtime.input().bind(Action::MoveLeft, KeyCode::A);
    runtime.input().bind(Action::MoveRight, KeyCode::D);
    runtime.input().bind(Action::Confirm, KeyCode::Enter);
    runtime.input().bind(Action::Fire, KeyCode::Space);
    runtime.input().bindGamepad(Action::Fire, 0, rlge::GamepadButton::A);
    runtime.input().bindAxis(Action::MoveLeft, 0, rlge::GamepadAxis::LeftX);
    runtime.input().bindAxis(Action::MoveRight, 0, rlge::GamepadAxis::RightX);

    breakout::BreakoutGame game(runtime, "../examples/breakout/assets/levels.toml");

    game.start();
    runtime.run();

    return 0;
}
