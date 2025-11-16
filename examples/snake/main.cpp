#include "engine.hpp"
#include "snake_game.hpp"
#include "snake_scene.hpp"

using namespace rlge;

int main() {
    snake::Config cfg;

    Engine engine(
        snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification,
        snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification,
        60,
        "RLGE Snake");

    // Basic input bindings
    engine.input().bind("left", KEY_A);
    engine.input().bind("right", KEY_D);
    engine.input().bind("up", KEY_W);
    engine.input().bind("down", KEY_S);
    engine.input().bind("enter", KEY_ENTER);

    auto& camera = engine.services().camera();
    camera.setOffset({
        snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification / 2.0f,
        snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification / 2.0f
    });
    camera.follow({0.0f, 0.0f}, 1.0f);

    auto& bus = engine.services().events();
    bus.subscribe<snake::RestartGame>([&engine](const snake::RestartGame& _) {
        engine.popScene(); // pop GameOverScene
        engine.popScene(); // pop old GameScene
        engine.pushScene<snake::GameScene>();
    });

    engine.pushScene<snake::GameScene>();
    engine.run();

    return 0;
}

