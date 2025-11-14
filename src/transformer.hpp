#pragma once
#include "component.hpp"
#include "raylib.h"

namespace rlge {
    class Transform : public Component {
    public:
        explicit Transform(Entity& e)
            : Component(e), position{0,0}, rotation(0.0f), scale{1.0f,1.0f} {}

        Vector2 position;
        float   rotation;
        Vector2 scale;
    };
}
