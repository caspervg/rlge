#pragma once
#include <memory>
#include <type_traits>
#include <utility>

#include "asset.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "imgui.h"
#include "input.hpp"
#include "prefab.h"
#include "raylib.h"
#include "render_queue.hpp"
#include "rlImGui.h"
#include "scene.hpp"
#include "tween.hpp"


namespace rlge {
    class GameServices {
    public:
        CollisionSystem& collisions() { return collisions_; }
        TweenSystem& tweens() { return tweens_; }
        AudioManager& audio() { return audio_; }
        PrefabFactory& prefabs() { return prefabs_; }
        Camera& camera() { return camera_; }

    private:
        CollisionSystem collisions_;
        TweenSystem tweens_;
        AudioManager audio_;
        PrefabFactory prefabs_;
        Camera camera_;

        friend class Engine;
    };

    class Engine {
    public:
        Engine(const int width, const int height, const int fps, const char* title) :
            running_(false) {
            SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
            InitWindow(width, height, title);
            SetTargetFPS(fps);
            rlImGuiSetup(true);
        }

        ~Engine() {
            scenes_ = SceneStack{}; // destroy scenes first
            assets_.unloadAll();
            rlImGuiShutdown();
            CloseWindow();
        }

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        template <typename T, typename... Args>
        void pushScene(Args&&... args) {
            static_assert(std::is_base_of_v<Scene, T>, "T must be Scene");
            auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
            scenes_.push(std::move(ptr));
        }

        void popScene() { scenes_.pop(); }

        void run() {
            running_ = true;
            while (running_ && !WindowShouldClose()) {
                const float dt = GetFrameTime();

                services_.tweens().update(dt);
                scenes_.update(dt);
                services_.collisions().update(dt);
                services_.audio().update();

                BeginDrawing();
                ClearBackground(BLACK);

                // Let scenes enqueue draw commands into the render queue.
                scenes_.draw();

                // Flush all draw commands with the active camera.
                renderer_.flush(services_.camera().camera());

                // Debug overlays via ImGui for scenes that opt in.
                rlImGuiBegin();
                ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
                scenes_.drawDebug();
                rlImGuiEnd();

                EndDrawing();
            }
        }

        void quit() { running_ = false; }

        AssetStore& assetStore() { return assets_; }
        const AssetStore& assetStore() const { return assets_; }

        Input& input() { return input_; }
        const Input& input() const { return input_; }

        RenderQueue& renderer() { return renderer_; }
        const RenderQueue& renderer() const { return renderer_; }

        GameServices& services() { return services_; }
        const GameServices& services() const { return services_; }

    private:
        bool running_;
        AssetStore assets_;
        Input input_;
        GameServices services_;
        RenderQueue renderer_;
        SceneStack scenes_;
    };
}
