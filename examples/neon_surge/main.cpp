// NEON SURGE - a twin-stick arena survival shooter built on RLGE.
//
// Engine features on display:
//   - Scene stack with fade transitions (menu -> arena -> game over)
//   - Entity/component model, trigger colliders with layer masks
//   - Layer shader (animated FBM nebula) + batched glow sprites
//   - Continuous & burst particle emitters, camera follow/shake/zoom-kick
//   - Scene-local + game-wide event buses, timers/cooldowns
//   - Procedural textures and fully synthesized audio (zero asset files)
#include "runtime.hpp"
#include "window.hpp"

#include "ns_config.hpp"
#include "ns_game.hpp"

int main() {
    using rlge::Action;
    using rlge::KeyCode;
    using rlge::WindowConfig;

    const WindowConfig wcfg{
        .width = neon::cfg.screenWidth,
        .height = neon::cfg.screenHeight,
        .fps = 120,
        .title = "NEON SURGE - RLGE demo",
        .resizable = true,
        .resizeMode = rlge::ResizeMode::Letterbox,
        .aspectRatio = neon::cfg.screenWidth / neon::cfg.screenHeight,
        .fullscreenKey = KeyCode::F11,
        .debugKey = KeyCode::F12,
    };
    rlge::Runtime runtime(wcfg);

    auto& input = runtime.input();
    // Movement: keyboard WASD falls back behind the left stick.
    input.bindAxis(Action::MoveRight, KeyCode::A, KeyCode::D);
    input.bindAxis(Action::MoveRight, 0, rlge::GamepadAxis::LeftX);
    input.bindAxis(Action::MoveDown, KeyCode::W, KeyCode::S);
    input.bindAxis(Action::MoveDown, 0, rlge::GamepadAxis::LeftY);

    // Fire: hold-friendly bindings across devices.
    input.bind(Action::Fire, KeyCode::Space);
    input.bindMouse(Action::Fire, rlge::MouseButton::Left);
    input.bindGamepad(Action::Fire, 0, rlge::GamepadButton::RightBumper);

    // Dash.
    input.bind(Action::Jump, KeyCode::LeftShift);
    input.bindMouse(Action::Jump, rlge::MouseButton::Right);
    input.bindGamepad(Action::Jump, 0, rlge::GamepadButton::A);

    // Menu confirm.
    input.bind(Action::Confirm, KeyCode::Enter);
    input.bindGamepad(Action::Confirm, 0, rlge::GamepadButton::Start);

    neon::NsGame game(runtime);
    game.start();
    runtime.run();

    return 0;
}
