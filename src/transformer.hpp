#pragma once
#include "component.hpp"
#include "raylib.h"
#include "raymath.h"

namespace rlge {
    class Transform : public Component {
    public:
        explicit Transform(Entity& e)
            : Component(e), position{0,0}, rotation(0.0f), scale{1.0f,1.0f} {}

        Vector2 position;
        float   rotation;
        Vector2 scale;

        [[nodiscard]] Matrix matrix() const {
            const auto t = MatrixTranslate(position.x, position.y, 0.0f);
            const auto r = MatrixRotateZ(rotation);
            const auto s = MatrixScale(scale.x, scale.y, 1.0f);

            // World = T * R * S
            return MatrixMultiply(s, MatrixMultiply(r, t));
        }

        [[nodiscard]] Vector2 right() const {
            return Vector2Rotate({1.0f, 0.0f}, rotation);
        }

        [[nodiscard]] Vector2 up() const {
            return Vector2Rotate({0.0f, 1.0f}, rotation);
        }
    };
}
