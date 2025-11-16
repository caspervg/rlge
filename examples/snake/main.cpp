#include "runtime.hpp"
#include "window.hpp"
#include "snake_game.hpp"
#include "snake_scene.hpp"

using namespace rlge;

int main() {
    snake::Config cfg;

    WindowConfig wcfg{
        .width = snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification,
        .height = snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification,
        .fps = 60,
        .title = "RLGE Snake"
    };
    Runtime runtime(wcfg);

    // Basic input bindings
    runtime.input().bind("left", KEY_A);
    runtime.input().bind("right", KEY_D);
    runtime.input().bind("up", KEY_W);
    runtime.input().bind("down", KEY_S);
    runtime.input().bind("enter", KEY_ENTER);

    auto& camera = runtime.services().camera();
    camera.setOffset({
        snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification / 2.0f,
        snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification / 2.0f
    });
    camera.follow({0.0f, 0.0f}, 1.0f);

    auto& bus = runtime.services().events();
    bus.subscribe<snake::RestartGame>([&runtime](const snake::RestartGame& _) {
        runtime.popScene(); // pop GameOverScene
        runtime.popScene(); // pop old GameScene
        runtime.pushScene<snake::GameScene>();
    });

    runtime.pushScene<snake::GameScene>();
    runtime.run();

    return 0;
}
