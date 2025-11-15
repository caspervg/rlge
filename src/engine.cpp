#include "engine.hpp"

#include "imgui.h"
#include "rlImGui.h"

namespace rlge {
    Engine::Engine(const int width, const int height, const int fps, const char* title)
        : running_(false) {
        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
        InitWindow(width, height, title);
        SetTargetFPS(fps);
        rlImGuiSetup(true);
    }

    Engine::~Engine() {
        scenes_ = SceneStack{};
        assets_.unloadAll();
        rlImGuiShutdown();
        CloseWindow();
    }

    void Engine::popScene() {
        scenes_.pop();
    }

    void Engine::run() {
        running_ = true;
        while (running_ && !WindowShouldClose()) {
            const float dt = GetFrameTime();

            services_.tweens().update(dt);
            scenes_.update(dt);
            services_.collisions().update(dt);
            services_.audio().update();

            BeginDrawing();
            ClearBackground(BLACK);

            scenes_.draw();

            renderer_.flush(services_.camera().camera());

            rlImGuiBegin();
            ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
            scenes_.drawDebug();
            rlImGuiEnd();

            EndDrawing();
        }
    }

    void Engine::quit() { running_ = false; }

    AssetStore& Engine::assetStore() { return assets_; }
    const AssetStore& Engine::assetStore() const { return assets_; }

    Input& Engine::input() { return input_; }
    const Input& Engine::input() const { return input_; }

    RenderQueue& Engine::renderer() { return renderer_; }
    const RenderQueue& Engine::renderer() const { return renderer_; }

    GameServices& Engine::services() { return services_; }
    const GameServices& Engine::services() const { return services_; }
}

