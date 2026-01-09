#include "hud.hpp"

#include "raylib.h"
#include "render_queue.hpp"

namespace anim_demo {
    Hud::Hud(rlge::Scene& scene, SeaCreature& hero)
        : RenderEntity(scene)
        , hero_(hero) {}

    void Hud::draw() {
        auto stateLabel = "Idle";
        switch (hero_.state()) {
            case AnimState::Idle:
                stateLabel = "Idle";
                break;
            case AnimState::Walk:
                stateLabel = "Walk";
                break;
            case AnimState::Attack:
                stateLabel = "Attack";
                break;
            case AnimState::Hurt:
                stateLabel = "Hurt";
                break;
            case AnimState::Death:
                stateLabel = "Death";
                break;
        }

        rq().submitUI([stateLabel] {
            DrawText("Sprite animation demo", 18, 16, 18, RAYWHITE);
            DrawText("A/D: walk, Space: attack, H: hurt, K: death, Enter: reset", 18, 40, 16, RAYWHITE);
            DrawText("Q/E: cycle variant", 18, 64, 16, RAYWHITE);
            DrawText(TextFormat("State: %s", stateLabel), 18, 88, 16, RAYWHITE);
        });

        if (hero_.eventTimer() > 0.0f) {
            const auto& label = hero_.lastEvent();
            rq().submitUI([label] {
                DrawText(TextFormat("Event: %s", label.c_str()), 18, 136, 16, RAYWHITE);
            });
        }
    }
}
