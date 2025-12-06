#include "breakout_scene.hpp"
#include "runtime.hpp"
#include "window.hpp"

using namespace rlge;

int main() {
    const WindowConfig wcfg{
        .width = breakout::g_cfg.viewPortWidth,
        .fps = 144,
        .title = "RLGE Breakout",
        .resizable = true,
        .startFullscreen = false,
        .aspectRatio = 16.0f / 9.0f,
        .fullscreenKey = KeyCode::F11,
        .debugKey = KeyCode::F12,
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

    // Use a debug-focused level set when not building with NDEBUG.
#ifdef NDEBUG
    constexpr auto levelFile = "../examples/breakout/assets/levels.toml";
#else
    constexpr auto levelFile = "../examples/breakout/assets/levels_debug.toml";
#endif

    breakout::BreakoutGame game(runtime, levelFile);

    game.start();
    runtime.run();

    return 0;
}
