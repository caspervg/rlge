#pragma once
#include "raylib.h"

namespace rlge {
    class Camera2DController {
    public:
        Camera2DController();

        void follow(Vector2 pos, float lerp = 0.1f);

        void setZoom(float z);
        [[nodiscard]] float zoom() const;
        void setRotation(float r);
        [[nodiscard]] float rotation() const;
        void setOffset(Vector2 o);
        [[nodiscard]] Vector2 offset() const;
        void setTarget(Vector2 t);
        [[nodiscard]] Vector2 target() const;
        void pan(Vector2 delta);
        void pan(float dx, float dy);

        void update(float dt);

        // Coordinate transforms
        [[nodiscard]] Vector2 screenToWorld(Vector2 screen) const;
        [[nodiscard]] Vector2 worldToScreen(Vector2 world) const;
        [[nodiscard]] Vector2 screenToWorld(float x, float y) const;
        [[nodiscard]] Vector2 worldToScreen(float x, float y) const;
        [[nodiscard]] Vector2 mouseWorldPosition() const;

        Camera2D& cam2d();
        [[nodiscard]] const Camera2D& cam2d() const;

        // Get world-space view bounds for frustum culling
        Rectangle getViewBounds() const;
        [[nodiscard]] Vector2 getShakeOffset() const;

        // Check if a point/rectangle is visible
        [[nodiscard]] bool isVisible(Vector2 point) const;
        [[nodiscard]] bool isVisible(const Rectangle& rect) const;

        void shake(float intensity, float duration);

    private:
        void updateShake_(float dt);

    private:
        Camera2D cam_{};
        float shakeIntensity_{0.0f};
        float shakeDuration_{0.0f};
        float shakeTimer_{0.0f};
        Vector2 shakeOffset_{0.0f, 0.0f};
        Vector2 originalOffset_{0.0f, 0.0f};
    };

    class Camera3DController {
    public:
        Camera3DController();

        void setPosition(const Vector3& p);
        [[nodiscard]] Vector3 position() const;
        void setTarget(const Vector3& t);
        [[nodiscard]] Vector3 target() const;
        void setUp(const Vector3& u);
        [[nodiscard]] Vector3 up() const;
        void setFovy(float f);
        [[nodiscard]] float fovy() const;
        void setProjection(CameraProjection proj);
        [[nodiscard]] CameraProjection projection() const;

        // Move both position and target by delta
        void move(const Vector3& delta);
        void move(float dx, float dy, float dz);

        // Orbit around current target with yaw/pitch (radians) and radius
        void orbitTarget(float yaw, float pitch, float radius);

        void update(float dt);
        void shake(float intensity, float duration);
        [[nodiscard]] Vector3 getShakeOffset() const;

        // Helpers
        [[nodiscard]] Ray mouseRay(Vector2 screen) const;
        [[nodiscard]] Ray mouseRay(const Rectangle& viewport, Vector2 screen) const;
        [[nodiscard]] Ray mouseRay(const Rectangle& viewport) const;
        [[nodiscard]] Ray mouseRay() const;
        [[nodiscard]] Vector2 worldToScreen(const Vector3& world) const;

        Camera3D& cam3d();
        [[nodiscard]] const Camera3D& cam3d() const;

    private:
        void updateShake_(float dt);

    private:
        Camera3D cam_{};
        Vector3 basePosition_{};
        Vector3 baseTarget_{};
        float shakeIntensity_{0.0f};
        float shakeDuration_{0.0f};
        float shakeTimer_{0.0f};
        Vector3 shakeOffset_{0.0f, 0.0f, 0.0f};
    };
}
