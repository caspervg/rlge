#include "camera.hpp"

namespace rlge {
    Camera::Camera() {
        cam_.target = {0, 0};
        cam_.offset = {480, 270};
        cam_.rotation = 0.0f;
        cam_.zoom = 1.0f;
    }

    void Camera::follow(const Vector2 pos, const float lerp) {
        cam_.target.x += (pos.x - cam_.target.x) * lerp;
        cam_.target.y += (pos.y - cam_.target.y) * lerp;
    }

    void Camera::setZoom(const float z) { cam_.zoom = z; }
    void Camera::setRotation(const float r) { cam_.rotation = r; }
    void Camera::setOffset(const Vector2 o) { cam_.offset = o; }

    Camera2D& Camera::camera() { return cam_; }
    const Camera2D& Camera::camera() const { return cam_; }
}

