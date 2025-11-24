#include "breakout_game.hpp"
#include "breakout_scene.cpp"
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
    runtime.input().bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
    runtime.input().bind(rlge::Action::MoveRight, rlge::KeyCode::D);
    runtime.input().bind(rlge::Action::MoveUp, rlge::KeyCode::W);
    runtime.input().bind(rlge::Action::MoveDown, rlge::KeyCode::S);
    runtime.input().bind(rlge::Action::Confirm, rlge::KeyCode::Enter);

    auto& bus = runtime.services().gameEvents();
    // bus.subscribe<snake::RestartGame>([&runtime](const snake::RestartGame& _) {
    //     runtime.popScene(); // pop GameOverScene
    //     runtime.popScene(); // pop old GameScene
    //     runtime.pushScene<snake::GameScene>();
    // });

    runtime.pushScene<breakout::BreakoutScene>();
    runtime.run();

    return 0;
}
