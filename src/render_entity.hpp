#pragma once
#include "engine.hpp"
#include "entity.hpp"

namespace rlge {
    class RenderEntity : public Entity {
    protected:
        explicit RenderEntity(Scene& s)
            : Entity(s) {}

        RenderQueue& rq() {
            return scene().engine().renderer();
        }

        const RenderQueue& rq() const {
            return scene().engine().renderer();
        }
    };
}

