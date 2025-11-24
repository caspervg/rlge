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

    // Basic input bindings
    runtime.input().bind("left", KEY_A);
    runtime.input().bind("right", KEY_D);
    runtime.input().bind("up", KEY_W);
    runtime.input().bind("down", KEY_S);
    runtime.input().bind("enter", KEY_ENTER);

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
