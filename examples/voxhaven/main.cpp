// VOXHAVEN - a first-person voxel sandbox built on RLGE's 3D lane.
//
// Engine features on display:
//   - addView3D with per-view Camera3D + layered submit3D commands
//     (sky -> terrain -> water/highlight/debris across background/world/foreground)
//   - Custom vertex+fragment shaders (fog, day tint, water waves) loaded
//     from memory through the AssetStore
//   - Scene-local event bus decoupling world edits from feedback effects
//   - Engine timers for autosave and ambient audio
//   - ImGui debug overlay (F12) with a time-of-day slider
//   - Procedural texture atlas + fully synthesized audio: zero asset files
//
// The world streams in chunks around the player, persists edits to
// voxhaven.world, and regenerates the same terrain from its saved seed.
#include "runtime.hpp"
#include "window.hpp"

#include "vx_config.hpp"
#include "vox_scene.hpp"

int main() {
    using rlge::Action;
    using rlge::KeyCode;

    const rlge::WindowConfig wcfg{
        .width = vox::cfg.screenWidth,
        .height = vox::cfg.screenHeight,
        .fps = 120,
        .title = "VOXHAVEN - RLGE demo",
        .flags = FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT,
        .resizable = true,
        .fullscreenKey = KeyCode::F11,
        .debugKey = KeyCode::F12,
    };
    rlge::Runtime runtime(wcfg);

    // Movement axes ride the engine input system (keyboard + gamepad ready).
    auto& input = runtime.input();
    input.bindAxis(Action::MoveRight, KeyCode::A, KeyCode::D);
    input.bindAxis(Action::MoveRight, 0, rlge::GamepadAxis::LeftX);
    input.bindAxis(Action::MoveDown, KeyCode::W, KeyCode::S);
    input.bindAxis(Action::MoveDown, 0, rlge::GamepadAxis::LeftY);

    runtime.pushScene<vox::VoxScene>();
    runtime.run();

    return 0;
}
