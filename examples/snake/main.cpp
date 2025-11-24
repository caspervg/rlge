#include "runtime.hpp"
#include "window.hpp"
#include "snake_game.hpp"
#include "snake_scene.hpp"

using namespace rlge;

int main() {
    snake::Config cfg;

    const WindowConfig wcfg{
        .width = snake::kTilesX * snake::kPixelsPerTile * snake::kMagnification,
        .height = snake::kTilesY * snake::kPixelsPerTile * snake::kMagnification,
        .fps = 144,
        .title = "RLGE Snake"
    };
    Runtime runtime(wcfg);

    // Basic input bindings using type-safe enums
    runtime.input().bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
    runtime.input().bind(rlge::Action::MoveRight, rlge::KeyCode::D);
    runtime.input().bind(rlge::Action::MoveUp, rlge::KeyCode::W);
    runtime.input().bind(rlge::Action::MoveDown, rlge::KeyCode::S);
    runtime.input().bind(rlge::Action::Confirm, rlge::KeyCode::Enter);

    auto& bus = runtime.services().gameEvents();
    bus.subscribe<snake::RestartGame>([&runtime](const snake::RestartGame& _) {
        runtime.popScene(); // pop GameOverScene
        runtime.popScene(); // pop old GameScene
        runtime.pushScene<snake::GameScene>();
    });

    runtime.pushScene<snake::GameScene>();
    runtime.run();

    return 0;
}
