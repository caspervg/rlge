#include "breakout_scene.hpp"
#include "game_over_scene.h"
#include "runtime.hpp"
#include "window.hpp"

using namespace rlge;

int main() {
    const WindowConfig wcfg{
        .width = breakout::g_cfg.width,
        .height = breakout::g_cfg.height,
        .fps = 144,
        .title = "RLGE Breakout"
    };
    Runtime runtime(wcfg);

    // Basic input bindings using type-safe enums
    runtime.input().bind(Action::MoveLeft, KeyCode::A);
    runtime.input().bind(Action::MoveRight, KeyCode::D);
    runtime.input().bind(Action::Confirm, KeyCode::Enter);
    runtime.input().bindAxis(Action::MoveLeft, 0, rlge::GamepadAxis::LeftX);
    runtime.input().bindAxis(Action::MoveRight, 0, rlge::GamepadAxis::RightX);

    auto& bus = runtime.services().gameEvents();
    bus.subscribe<breakout::RestartGame>([&runtime](const breakout::RestartGame& _) {
        runtime.popScene(); // pop GameOverScene
        runtime.popScene(); // pop old BreakoutScene
        runtime.pushScene<breakout::BreakoutScene>();
    });
    bus.subscribe<breakout::GameLost>([&runtime](const breakout::GameLost& e) {
        runtime.popScene(); // pop BreakoutScene
        runtime.pushScene<breakout::GameOverScene>(e.finalScore);
    });

    runtime.pushScene<breakout::BreakoutScene>();
    runtime.run();

    return 0;
}
