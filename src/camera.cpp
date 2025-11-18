#include "camera.hpp"

namespace rlge {
    Camera::Camera() {
        cam_.target = {0, 0};
        cam_.offset = {0, 0};
        cam_.rotation = 0.0f;
        cam_.zoom = 1.0f;
    }

    void Camera::follow(const Vector2 pos, const float lerp) {
        cam_.target.x += (pos.x - cam_.target.x) * lerp;
        cam_.target.y += (pos.y - cam_.target.y) * lerp;
    }

    void Camera::setZoom(const float z) { cam_.zoom = z; }
    float Camera::zoom() const { return cam_.zoom; }
    void Camera::setRotation(const float r) { cam_.rotation = r; }
    float Camera::rotation() const { return cam_.rotation; }
    void Camera::setOffset(const Vector2 o) { cam_.offset = o; }
    Vector2 Camera::offset() const { return cam_.offset; }
    void Camera::setTarget(const Vector2 t) { cam_.target = t; }
    Vector2 Camera::target() const { return cam_.target; }

    void Camera::pan(const Vector2 delta) {
        cam_.target.x += delta.x;
        cam_.target.y += delta.y;
    }
    void Camera::pan(const float dx, const float dy) { pan({dx, dy}); }

    Vector2 Camera::screenToWorld(const Vector2 screen) const {
        return GetScreenToWorld2D(screen, cam_);
    }

    Vector2 Camera::worldToScreen(const Vector2 world) const {
        return GetWorldToScreen2D(world, cam_);
    }

    Vector2 Camera::screenToWorld(const float x, const float y) const {
        return screenToWorld(Vector2{x, y});
    }

    Vector2 Camera::worldToScreen(const float x, const float y) const {
        return worldToScreen(Vector2{x, y});
    }

    Vector2 Camera::mouseWorldPosition() const {
        return GetScreenToWorld2D(GetMousePosition(), cam_);
    }

    Camera2D& Camera::cam2d() { return cam_; }
    const Camera2D& Camera::cam2d() const { return cam_; }

    Rectangle Camera::getViewBounds() const {
        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();

        // Calculate world-space bounds
        const Vector2 topLeft = GetScreenToWorld2D({0, 0}, cam_);
        const Vector2 bottomRight = GetScreenToWorld2D(
            {static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, cam_);

        return Rectangle{
            topLeft.x,
            topLeft.y,
            bottomRight.x - topLeft.x,
            bottomRight.y - topLeft.y
        };
    }

    bool Camera::isVisible(const Vector2 point) const {
        const auto bounds = getViewBounds();
        return CheckCollisionPointRec(point, bounds);
    }

    bool Camera::isVisible(const Rectangle& rect) const {
        const auto bounds = getViewBounds();
        return CheckCollisionRecs(rect, bounds);
    }
}
