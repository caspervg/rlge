#pragma once
#include "raylib.h"

namespace rlge {
    class Camera {
    public:
        Camera();

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
}
