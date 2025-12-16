#include <algorithm>
#include <array>

#include "raylib.h"
#include "runtime.hpp"
#include "scene.hpp"

namespace demo {
    class HybridScene : public rlge::Scene {
    public:
        using Scene::Scene;

        void enter() override {
            Scene::enter();

            auto [w, h] = runtime().window().size();
            setupCameras_(w, h);
            loadResources_();

            applyViewLayout_(w, h);
        }

        void exit() override {
            if (billboard_.id != 0) {
                UnloadTexture(billboard_);
                billboard_.id = 0;
            }
            if (cubeModel_.meshCount > 0) {
                UnloadModel(cubeModel_);
                cubeModel_.meshCount = 0;
            }
            Scene::exit();
        }

        void update(const float dt) override {
            Scene::update(dt);

            cubeRotation_ += dt * 45.0f;
            orbitAngle_ += dt * orbitSpeed_ * (IsKeyDown(KEY_RIGHT) ? 1.0f : IsKeyDown(KEY_LEFT) ? -1.0f : 0.0f);
            camHeight_ += dt * heightSpeed_ * (IsKeyDown(KEY_UP) ? 1.0f : IsKeyDown(KEY_DOWN) ? -1.0f : 0.0f);
            camHeight_ = std::clamp(camHeight_, 1.0f, 6.0f);

            if (IsKeyPressed(KEY_B)) {
                showBillboards_ = !showBillboards_;
            }
            if (IsKeyPressed(KEY_O)) {
                show2DOverlay_ = !show2DOverlay_;
            }

            updateCameraPosition_();
        }

        void draw() override {
            Scene::draw();

            // 3D content: grid, a rotating cube, and some billboards
            rq().submitWorld([this] {
                DrawGrid(24, 1.0f);
            });

            rq().submit3D(layers().world(), [this](const Camera3D&, const Rectangle&) {
                const Vector3 cubePos{0.0f, 0.75f, 0.0f};
                const Vector3 axis{0.0f, 1.0f, 0.0f};
                DrawModelEx(cubeModel_, cubePos, axis, cubeRotation_, {1.0f, 1.0f, 1.0f}, RED);
            });

            // Use the helper to place a static cube to the side.
            rq().submitModel(layers().world(), cubeModel_, {2.5f, 0.5f, -2.5f}, 0.75f, BLUE);

            if (showBillboards_) {
                static constexpr std::array billboardPositions{
                    Vector3{-3.5f, 1.2f, -2.0f},
                    Vector3{-2.5f, 1.2f, 2.5f},
                    Vector3{2.5f, 1.2f, 2.5f},
                    Vector3{3.5f, 1.2f, -1.5f}
                };
                for (const auto& pos : billboardPositions) {
                    rq().submitBillboard(layers().world(), billboard_, pos, 1.0f, WHITE);
                }
            }

            // 2D overlay in world space (uses the overlay 2D camera on the same view)
            if (show2DOverlay_) {
                rq().submitWorld(0.1f, [this] {
                    const auto* viewCtx = rq().currentView();
                    const bool isInset = viewCtx && viewCtx->camera2d == &overlayCamMini_.cam2d();
                    const Color base = isInset ? ColorAlpha(LIME, 0.7f) : ColorAlpha(YELLOW, 0.7f);
                    const Color accent = isInset ? ColorAlpha(SKYBLUE, 0.6f) : ColorAlpha(ORANGE, 0.7f);
                    DrawCircleLines(0, 0, 80, base);
                    DrawLine(-120, 0, 120, 0, base);
                    DrawLine(0, -120, 0, 120, base);
                    DrawRectangleLines(-30, -30, 60, 60, accent);
                });
            }

            // Screen-space UI
            rq().submitUI([this] {
                DrawText("Hybrid 3D + 2D views (main + inset)", 10, 10, 22, RAYWHITE);
                DrawText("Left/Right: orbit camera  |  Up/Down: adjust height", 10, 38, 18, RAYWHITE);
                DrawText("B: toggle billboards  |  O: toggle 2D overlay", 10, 60, 18, RAYWHITE);
                DrawRectangleLinesEx(insetViewport_, 2.0f, ColorAlpha(RAYWHITE, 0.8f));
                DrawText("Inset", static_cast<int>(insetViewport_.x + 8), static_cast<int>(insetViewport_.y + 8), 16, RAYWHITE);
            });
        }

    private:
        void setupCameras_(float w, float h) {
            cam3d_.setPosition({6.0f, 3.0f, 6.0f});
            cam3d_.setTarget({0.0f, 1.0f, 0.0f});
            cam3d_.setUp({0.0f, 1.0f, 0.0f});
            cam3d_.setFovy(50.0f);
            cam3d_.setProjection(CAMERA_PERSPECTIVE);

            overlayCam_.setOffset({w * 0.5f, h * 0.5f});
            overlayCam_.setTarget({0.0f, 0.0f});
            overlayCam_.setZoom(80.0f);

            // Top-down inset view
            cam3dTop_.setTarget({0.0f, 0.0f, 0.0f});
            cam3dTop_.setUp({0.0f, 0.0f, -1.0f});
            cam3dTop_.setFovy(45.0f);
            cam3dTop_.setProjection(CAMERA_PERSPECTIVE);

            overlayCamMini_.setTarget({0.0f, 0.0f});
            overlayCamMini_.setZoom(50.0f);
        }

        void loadResources_() {
            const Image img = GenImageChecked(64, 64, 8, 8, DARKGREEN, LIME);
            billboard_ = LoadTextureFromImage(img);
            UnloadImage(img);

            cubeModel_ = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
        }

        void updateCameraPosition_() {
            constexpr auto radius = 6.0f;
            const float yaw = orbitAngle_;
            const float pitch = std::clamp(camHeight_ * 0.1f, -1.2f, 1.2f);
            cam3d_.setTarget({0.0f, 1.0f, 0.0f});
            cam3d_.orbitTarget(yaw, pitch, radius);

            cam3dTop_.setTarget({0.0f, 0.0f, 0.0f});
            cam3dTop_.setPosition({0.0f, 8.0f, 0.001f}); // slight Z offset to avoid singularity
        }

        void applyViewLayout_(float w, float h) {
            runtime().clearViews();
            insetViewport_ = {};

            runtime().addView3D(
                cam3d_,
                Rectangle{0, 0, w, h},
                [this](float width, float height) {
                    overlayCam_.setOffset({width * 0.5f, height * 0.5f});
                    return Rectangle{0, 0, width, height};
                },
                std::nullopt,
                std::nullopt,
                &overlayCam_);

            constexpr auto margin = 16.0f;
            const float miniW = w * 0.3f;
            const float miniH = h * 0.3f;
            const float miniX = w - miniW - margin;
            constexpr float miniY = margin;
            insetViewport_ = {miniX, miniY, miniW, miniH};
            overlayCamMini_.setOffset({miniW * 0.5f, miniH * 0.5f});
            overlayCamMini_.setZoom(50.0f);

            runtime().addView3D(
                cam3dTop_,
                Rectangle{miniX, miniY, miniW, miniH},
                [this](const float width, const float height) {
                    const float mw = width * 0.3f;
                    const float mh = height * 0.3f;
                    overlayCamMini_.setOffset({mw * 0.5f, mh * 0.5f});
                    overlayCamMini_.setZoom(50.0f);
                    insetViewport_ = {width - mw - margin, margin, mw, mh};
                    return Rectangle{width - mw - margin, margin, mw, mh};
                },
                std::nullopt,
                std::nullopt,
                &overlayCamMini_);
        }

    private:
        rlge::Camera3DController cam3d_{};
        rlge::Camera3DController cam3dTop_{};
        rlge::Camera2DController overlayCam_{};
        rlge::Camera2DController overlayCamMini_{};
        Model cubeModel_{};
        Texture2D billboard_{};
        float cubeRotation_{0.0f};
        float orbitAngle_{0.0f};
        float camHeight_{3.0f};
        float orbitSpeed_{1.5f};
        float heightSpeed_{2.0f};
        bool showBillboards_{true};
        bool show2DOverlay_{true};
        Rectangle insetViewport_{};
    };
} // namespace demo

int main() {
    rlge::WindowConfig cfg{
        .width = 1280,
        .height = 720,
        .fps = 60,
        .title = "RLGE Hybrid 3D/2D",
        .resizable = true,
    };

    rlge::Runtime runtime(cfg);
    runtime.pushScene<demo::HybridScene>();
    runtime.run();
    return 0;
}
