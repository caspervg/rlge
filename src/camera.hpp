#pragma once
#include "raylib.h"

namespace rlge {
    class Camera {
    public:
        Camera();

        void follow(Vector2 pos, float lerp = 0.1f);

        void setZoom(float z);
        void setRotation(float r);
        void setOffset(Vector2 o);

        Camera2D& camera();
        const Camera2D& camera() const;

    private:
        Camera2D cam_{};
    };
}
