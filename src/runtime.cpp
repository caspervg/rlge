#include "runtime.hpp"

#include <algorithm>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

namespace rlge {
    Runtime::Runtime(const WindowConfig& cfg) : window_(cfg) { rlImGuiSetup(true); }

    Runtime::~Runtime() {
        scenes_ = SceneStack{};
        assets_.unloadAll();
        rlImGuiShutdown();
    }

    void Runtime::popScene() { scenes_.pop(); }

    void Runtime::run() {
        running_ = true;
        while (running_ && !WindowShouldClose()) {
            renderer_.beginFrame();
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

            renderer_.prepareWorld();

            for (const auto& view : views_) {
                if (!view.camera)
                    continue;

                BeginScissorMode(static_cast<int>(view.viewport.x), static_cast<int>(view.viewport.y),
                                 static_cast<int>(view.viewport.width), static_cast<int>(view.viewport.height));

                renderer_.flushPreparedWorld(view.camera->cam2d(), view.viewport);

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

    ViewId Runtime::addView(Camera& camera, const Rectangle& viewport) {
        const ViewId id = nextViewId_++;
        views_.push_back(View{id, &camera, viewport});
        return id;
    }

    void Runtime::clearViews() { views_.clear(); }

    bool Runtime::removeView(ViewId id) {
        const auto it = std::ranges::find_if(views_, [id](const View& v) { return v.id == id; });
        if (it != views_.end()) {
            views_.erase(it);
            return true;
        }
        return false;
    }

    View* Runtime::primaryView() {
        if (views_.empty())
            return nullptr;
        return &views_.front();
    }

    const View* Runtime::primaryView() const {
        if (views_.empty())
            return nullptr;
        return &views_.front();
    }

    View* Runtime::view(ViewId id) {
        const auto it = std::ranges::find_if(views_, [id](const View& v) { return v.id == id; });
        if (it != views_.end()) {
            return &(*it);
        }
        return nullptr;
    }

    const View* Runtime::view(ViewId id) const {
        const auto it = std::ranges::find_if(views_, [id](const View& v) { return v.id == id; });
        if (it != views_.end()) {
            return &(*it);
        }
        return nullptr;
    }
    const std::vector<View>& Runtime::views() const { return views_; }
} // namespace rlge
