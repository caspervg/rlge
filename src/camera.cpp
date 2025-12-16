#include "camera.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace rlge {
    Camera2DController::Camera2DController() {
        cam_.target = {0, 0};
        cam_.offset = {0, 0};
        cam_.rotation = 0.0f;
        cam_.zoom = 1.0f;
    }

    void Camera2DController::follow(const Vector2 pos, const float lerp) {
        cam_.target.x += (pos.x - cam_.target.x) * lerp;
        cam_.target.y += (pos.y - cam_.target.y) * lerp;
    }

    void Camera2DController::setZoom(const float z) { cam_.zoom = z; }
    float Camera2DController::zoom() const { return cam_.zoom; }
    void Camera2DController::setRotation(const float r) { cam_.rotation = r; }
    float Camera2DController::rotation() const { return cam_.rotation; }
    void Camera2DController::setOffset(const Vector2 o) { cam_.offset = o; originalOffset_ = o; }
    Vector2 Camera2DController::offset() const { return cam_.offset; }
    void Camera2DController::setTarget(const Vector2 t) { cam_.target = t; }
    Vector2 Camera2DController::target() const { return cam_.target; }

    void Camera2DController::pan(const Vector2 delta) {
        cam_.target.x += delta.x;
        cam_.target.y += delta.y;
    }
    void Camera2DController::pan(const float dx, const float dy) { pan({dx, dy}); }

    void Camera2DController::update(const float dt) {
        updateShake_(dt);
        cam_.offset = Vector2Add(cam_.offset, shakeOffset_);
    }

    Vector2 Camera2DController::screenToWorld(const Vector2 screen) const {
        return GetScreenToWorld2D(screen, cam_);
    }

    Vector2 Camera2DController::worldToScreen(const Vector2 world) const {
        return GetWorldToScreen2D(world, cam_);
    }

    Vector2 Camera2DController::screenToWorld(const float x, const float y) const {
        return screenToWorld(Vector2{x, y});
    }

    Vector2 Camera2DController::worldToScreen(const float x, const float y) const {
        return worldToScreen(Vector2{x, y});
    }

    Vector2 Camera2DController::mouseWorldPosition() const {
        return GetScreenToWorld2D(GetMousePosition(), cam_);
    }

    Camera2D& Camera2DController::cam2d() { return cam_; }
    const Camera2D& Camera2DController::cam2d() const { return cam_; }

    Rectangle Camera2DController::getViewBounds() const {
        const int screenWidth = GetRenderWidth();
        const int screenHeight = GetRenderHeight();

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

    Vector2 Camera2DController::getShakeOffset() const {
        return shakeOffset_;
    }

    bool Camera2DController::isVisible(const Vector2 point) const {
        const auto bounds = getViewBounds();
        return CheckCollisionPointRec(point, bounds);
    }

    bool Camera2DController::isVisible(const Rectangle& rect) const {
        const auto bounds = getViewBounds();
        return CheckCollisionRecs(rect, bounds);
    }

    void Camera2DController::shake(const float intensity, const float duration) {
        shakeIntensity_ = std::max(shakeIntensity_, intensity);
        shakeDuration_ = duration;
        shakeTimer_ = 0.0f;
    }

    void Camera2DController::updateShake_(const float dt) {
        if (shakeTimer_ >= shakeDuration_) {
            shakeOffset_ = {0.0f, 0.0f};
            cam_.offset = originalOffset_;
            return;
        }

        shakeTimer_ += dt;

        // Decay over time
        auto const progress = shakeTimer_ / shakeDuration_;
        auto const currentIntensity = shakeIntensity_ * (1.0f - progress);
        shakeOffset_ = {
            shakeIntensity_ * 2.0f * (0.5f - GetRandomValue(0, currentIntensity)) * std::sin(progress * PI),
            shakeIntensity_ * 2.0f * (0.5f - GetRandomValue(0, currentIntensity)) * std::sin(progress * PI)
        };
    }

    Camera3DController::Camera3DController() {
        cam_.position = {0.0f, 2.0f, 4.0f};
        cam_.target = {0.0f, 1.0f, 0.0f};
        cam_.up = {0.0f, 1.0f, 0.0f};
        cam_.fovy = 60.0f;
        cam_.projection = CAMERA_PERSPECTIVE;
    }

    void Camera3DController::setPosition(const Vector3& p) { cam_.position = p; }
    Vector3 Camera3DController::position() const { return cam_.position; }
    void Camera3DController::setTarget(const Vector3& t) { cam_.target = t; }
    Vector3 Camera3DController::target() const { return cam_.target; }
    void Camera3DController::setUp(const Vector3& u) { cam_.up = u; }
    Vector3 Camera3DController::up() const { return cam_.up; }
    void Camera3DController::setFovy(const float f) { cam_.fovy = f; }
    float Camera3DController::fovy() const { return cam_.fovy; }
    void Camera3DController::setProjection(const CameraProjection proj) { cam_.projection = proj; }
    CameraProjection Camera3DController::projection() const { return static_cast<CameraProjection>(cam_.projection); }

    void Camera3DController::move(const Vector3 delta) {
        cam_.position = Vector3Add(cam_.position, delta);
        cam_.target = Vector3Add(cam_.target, delta);
    }
    void Camera3DController::move(const float dx, const float dy, const float dz) { move(Vector3{dx, dy, dz}); }

    void Camera3DController::orbitTarget(const float yaw, const float pitch, const float radius) {
        const float cp = std::cosf(pitch);
        const float sp = std::sinf(pitch);
        const float cy = std::cosf(yaw);
        const float sy = std::sinf(yaw);
        const Vector3 offset{
            cy * cp * radius,
            sp * radius,
            sy * cp * radius
        };
        cam_.position = Vector3Add(cam_.target, offset);
    }

    Ray Camera3DController::mouseRay(const Vector2 screen) const {
        return GetMouseRay(screen, cam_);
    }

    Ray Camera3DController::mouseRay() const {
        return mouseRay(GetMousePosition());
    }

    Vector2 Camera3DController::worldToScreen(const Vector3& world) const {
        return GetWorldToScreen(world, cam_);
    }

    Camera3D& Camera3DController::cam3d() { return cam_; }
    const Camera3D& Camera3DController::cam3d() const { return cam_; }

}
