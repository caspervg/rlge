#include "runtime.hpp"

#include "imgui.h"
#include "rlImGui.h"

namespace rlge {
    Runtime::Runtime(const WindowConfig& cfg)
        : running_(false)
        , window_(cfg) {
        rlImGuiSetup(true);

        // Default full-screen view using the main camera
        const Vector2 size = window_.size();
        views_.push_back(View{
            &services_.camera(),
            Rectangle{0.0f, 0.0f, size.x, size.y}
        });
    }

    Runtime::~Runtime() {
        scenes_ = SceneStack{};
        assets_.unloadAll();
        rlImGuiShutdown();
    }

    void Runtime::popScene() {
        scenes_.pop();
    }

    void Runtime::clearViews() {
        views_.clear();
    }

    void Runtime::addView(Camera& camera, const Rectangle& viewport) {
        views_.push_back(View{&camera, viewport});
    }

    void Runtime::run() {
        running_ = true;
        while (running_ && !WindowShouldClose()) {
            const float dt = GetFrameTime();

            if (IsKeyPressed(debugKey_)) {
                debugEnabled_ = !debugEnabled_;
            }

            services_.tweens().update(dt);
            scenes_.update(dt);
            services_.collisions().update(dt);
            services_.audio().update();
            services_.events().dispatchQueued();

            BeginDrawing();
            ClearBackground(BLACK);

            scenes_.draw();

            // Render world for each active view (camera + viewport)
            for (const auto& view : views_) {
                if (!view.camera)
                    continue;

                BeginScissorMode(
                    static_cast<int>(view.viewport.x),
                    static_cast<int>(view.viewport.y),
                    static_cast<int>(view.viewport.width),
                    static_cast<int>(view.viewport.height));

                renderer_.flushWorld(view.camera->cam2d(), view.viewport);

                EndScissorMode();
            }

            // Render UI once, in screen space
            renderer_.flushUI();

            if (debugEnabled_) {
                rlImGuiBegin();
                ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
                scenes_.drawDebug();
                rlImGuiEnd();
            }

            EndDrawing();
        }
    }

    void Runtime::quit() { running_ = false; }

    AssetStore& Runtime::assetStore() { return assets_; }
    const AssetStore& Runtime::assetStore() const { return assets_; }

    Input& Runtime::input() { return input_; }
    const Input& Runtime::input() const { return input_; }

    RenderQueue& Runtime::renderer() { return renderer_; }
    const RenderQueue& Runtime::renderer() const { return renderer_; }

    GameServices& Runtime::services() { return services_; }
    const GameServices& Runtime::services() const { return services_; }

    Window& Runtime::window() { return window_; }
    const Window& Runtime::window() const { return window_; }
}
