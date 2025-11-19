#pragma once
#include "entity.hpp"
#include "scene.hpp"

namespace rlge {
    class RenderQueue;

    class RenderEntity : public Entity {
    protected:
        explicit RenderEntity(Scene& s)
            : Entity(s) {}

        RenderQueue& rq() {
            return scene().rq();
        }

        [[nodiscard]] const RenderQueue& rq() const {
            return scene().rq();
        }

        EventBus& events() {
            return scene().events();
        }

        AudioManager& audio() {
            return scene().audio();
        }

        Input& input() {
            return scene().input();
        }

        AssetStore& assets() {
            return scene().assets();
        }
    };
}
