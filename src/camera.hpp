#pragma once
#include "raylib.h"

namespace rlge {
    class Camera {
    public:
        Camera() {
            cam_.target = {0, 0};
            cam_.offset = {480, 270};
            cam_.rotation = 0.0f;
            cam_.zoom = 1.0f;
        }

        void follow(const Vector2 pos, const float lerp = 0.1f) {
            cam_.target.x += (pos.x - cam_.target.x) * lerp;
            cam_.target.y += (pos.y - cam_.target.y) * lerp;
        }

        void setZoom(const float z) { cam_.zoom = z; }
        void setRotation(const float r) { cam_.rotation = r; }
        void setOffset(const Vector2 o) { cam_.offset = o; }

        Camera2D& camera() { return cam_; }
        const Camera2D& camera() const { return cam_; }

    private:
        Camera2D cam_;
    };
}
