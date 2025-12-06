#include "runtime.hpp"
#include "scene.hpp"

#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

namespace rlge {
    Runtime::Runtime(const WindowConfig& cfg)
        : window_(cfg)
        , renderer_(layers_) {
        layers_.createDefaults();
        assets_.setRoot(std::filesystem::current_path());

        aspectRatio_ = cfg.aspectRatio > 0 ? cfg.aspectRatio : (cfg.width / cfg.height);
        lastWidth_ = cfg.width;
        lastHeight_ = cfg.aspectRatio > 0 ? cfg.width / aspectRatio_ : cfg.height;
        fullscreenKey_ = cfg.fullscreenKey;
        debugKey_ = cfg.debugKey;

        rlImGuiSetup(true);
    }

    Runtime::~Runtime() {
        scenes_ = SceneStack{};
        assets_.unloadAll();
        rlImGuiShutdown();
    }

    void Runtime::popScene() { scenes_.pop(); }

    void Runtime::clearScenes() {
        scenes_.clear();
    }

    void Runtime::run() {
        running_ = true;
        while (running_ && !WindowShouldClose()) {
            renderer_.beginFrame();
            const float dt = GetFrameTime();

            if (fullscreenKey_.has_value() && input_.keyPressed(*fullscreenKey_)) {
                window_.toggleFullscreen();
            }

            const float w = GetScreenWidth();
            const float h = GetScreenHeight();
            if (lastWidth_ != w || lastHeight_ != h) {
                handleResize_(w, h);
                lastWidth_ = w;
                lastHeight_ = h;
            }

            if (debugKey_.has_value() && input_.keyPressed(*debugKey_)) {
                debugEnabled_ = !debugEnabled_;
            }

            if (transitionState_ != TransitionState::None) {
                updateTransition_(dt);
            }
            else {
                scenes_.update(dt);
            }
            services_.timers().update(dt);
            services_.audio().update();
            services_.gameEvents().dispatchQueued();

            Scene* activeScene = scenes_.top();

            BeginDrawing();
            ClearBackground(BLACK);

            scenes_.draw();

            renderer_.prepareWorld();

            RenderTexture2D* worldTarget = nullptr;
            if (activeScene) {
                worldTarget = activeScene->beginWorldRenderTarget();
            }

            const bool usingTarget = worldTarget && worldTarget->texture.id != 0;
            if (usingTarget) {
                BeginTextureMode(*worldTarget);
                ClearBackground(BLACK);
            }

            for (const auto& view : views_) {
                Camera& cam = view.camera.get();
                cam.update(dt);

                BeginScissorMode(static_cast<int>(view.viewport.x), static_cast<int>(view.viewport.y),
                                 static_cast<int>(view.viewport.width), static_cast<int>(view.viewport.height));

                renderer_.flushPreparedWorld(cam.cam2d(), view.viewport);

                EndScissorMode();
            }

            if (usingTarget) {
                EndTextureMode();
            }

            if (activeScene) {
                activeScene->afterWorldRender(worldTarget, views_);
            }

            if (transitionState_ != TransitionState::None) {
                drawTransition_();
            }
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

    Input<>& Runtime::input() { return input_; }

    const Input<>& Runtime::input() const { return input_; }

    RenderQueue& Runtime::renderer() { return renderer_; }

    const RenderQueue& Runtime::renderer() const { return renderer_; }

    LayerRegistry& Runtime::layers() { return layers_; }

    const LayerRegistry& Runtime::layers() const { return layers_; }

    GameServices& Runtime::services() { return services_; }

    const GameServices& Runtime::services() const { return services_; }

    Window& Runtime::window() { return window_; }

    const Window& Runtime::window() const { return window_; }

    ViewId Runtime::addView(Camera& camera, const Rectangle& viewport,
                            std::function<Rectangle(float width, float height)> onResize,
                            const std::optional<ResizeMode> mode,
                            const std::optional<float> aspectRatio) {
        const ViewId id = nextViewId_++;
        views_.push_back(View{id, std::ref(camera), viewport, std::move(onResize), mode, aspectRatio});
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

    void Runtime::setResizeMode(const ResizeMode mode, const std::optional<float> aspectRatio) {
        resizeMode_ = mode;
        aspectRatio_ = aspectRatio.value_or(aspectRatio_);
    }

    ResizeMode Runtime::resizeMode() const { return resizeMode_; }

    float Runtime::aspectRatio() const { return aspectRatio_; }

    void Runtime::onResize(std::function<void(float width, float height)> cb) { resizeCallbacks_.push_back(cb); }

    void Runtime::updateTransition_(const float dt) {
        if (pendingTransition_->update(dt)) {
            if (transitionState_ == TransitionState::Out) {
                // Switch scenes at the midpoint
                scenes_.pop();
                scenes_.push(pendingSceneFactory_());
                pendingTransition_->setPhase(TransitionPhase::In);
                transitionState_ = TransitionState::In;
            } else {
                // Transition complete
                pendingTransition_.reset();
                transitionState_ = TransitionState::None;
            }
        }
    }

    void Runtime::drawTransition_() {
        const auto [x, y] = window_.size();
        pendingTransition_->draw(renderer_, x, y);
    }

    void Runtime::handleResize_(const float width, const float height) {
        for (auto& view : views_) {
            const auto mode = view.resizeMode.value_or(resizeMode_);
            const auto aspect = view.aspectRatio.value_or(aspectRatio_);

            if (view.onResize) {
                view.viewport = view.onResize(width, height);
            }
            else {
                if (mode == ResizeMode::Fill) {
                    view.viewport = {0, 0, width, height};
                }
                else {
                    // Letterbox
                    const float windowAspect = width / height;
                    float vw = width, vh = height, vx = 0.f, vy = 0.f;

                    if (windowAspect > aspect) {
                        vw = height * aspect;
                        vx = (width - vw) * 0.5f;
                    }
                    else if (windowAspect < aspect) {
                        vh = width / aspect;
                        vy = (height - vh) * 0.5f;
                    }

                    view.viewport = {vx, vy, vw, vh};
                }
            }
        }

        for (auto& cb : resizeCallbacks_) {
            if (cb) {
                cb(width, height);
            }
        }
    }


} // namespace rlge
