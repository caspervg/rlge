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

    breakout::LevelManager levelManager("../examples/breakout/assets/levels.toml");

    auto& bus = runtime.services().gameEvents();
    bus.subscribe<breakout::RestartGame>([&runtime, &levelManager](const breakout::RestartGame& _) {
        levelManager.reset();
        runtime.popScene(); // pop GameOverScene
        runtime.popScene(); // pop old BreakoutScene
        runtime.pushScene<breakout::BreakoutScene>(levelManager.currentLevel());
    });
    bus.subscribe<breakout::GameLost>([&runtime](const breakout::GameLost& e) {
        runtime.popScene(); // pop BreakoutScene
        runtime.pushScene<breakout::GameOverScene>(e.finalScore);
    });
    bus.subscribe<breakout::LevelCompleted>([&runtime, &levelManager](const breakout::LevelCompleted& _) {
        if (levelManager.nextLevel()) {
            runtime.transitionTo<breakout::BreakoutScene>(std::make_unique<FadeTransition>(0.35f), levelManager.currentLevel());
        }
    });

    runtime.pushScene<breakout::BreakoutScene>(levelManager.currentLevel());
    runtime.run();

    return 0;
}
