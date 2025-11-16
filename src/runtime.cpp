#include "runtime.hpp"

#include "imgui.h"
#include "rlImGui.h"

namespace rlge {
    Runtime::Runtime(const int width, const int height, const int fps, const char* title)
        : running_(false) {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
        InitWindow(width, height, title);
        SetTargetFPS(fps);
        rlImGuiSetup(true);
    }

    Runtime::~Runtime() {
        scenes_ = SceneStack{};
        assets_.unloadAll();
        rlImGuiShutdown();
        CloseWindow();
    }

    void Runtime::popScene() {
        scenes_.pop();
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

            renderer_.flush(services_.camera().camera());

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
}
