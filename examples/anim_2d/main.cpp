#include "action.hpp"
#include "anim_demo_scene.hpp"

#include "runtime.hpp"
#include "window.hpp"

using namespace rlge;

int main() {
    constexpr WindowConfig cfg{
        .width = 960,
        .height = 540,
        .fps = 60,
        .title = "RLGE Animations",
        .debugKey = KeyCode::F12
    };
    Runtime runtime(cfg);

    runtime.input().bind(Action::MoveLeft, KeyCode::A);
    runtime.input().bind(Action::MoveRight, KeyCode::D);
    runtime.input().bind(Action::Attack, KeyCode::Space);
    runtime.input().bind(Action::Hurt, KeyCode::H);
    runtime.input().bind(Action::Death, KeyCode::K);
    runtime.input().bind(Action::Reset, KeyCode::Enter);
    runtime.input().bind(Action::VariantPrev, KeyCode::Q);
    runtime.input().bind(Action::VariantNext, KeyCode::E);

    runtime.pushScene<anim_demo::AnimDemoScene>();
    runtime.run();

    return 0;
}
