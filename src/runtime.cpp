#include "runtime.hpp"
#include "scene.hpp"
#include "entity.hpp"

#include <algorithm>
#include <filesystem>

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "shader_effect.hpp"

namespace rlge {
    Runtime::Runtime(const WindowConfig& cfg)
        : window_(cfg)
        , renderer_(layers_) {
        layers_.createDefaults();
        assets_.setRoot(std::filesystem::current_path());
        assets_.addShaderReloadListener([this](const ShaderHandle handle, const bool success) {
            if (!success || !handle)
                return;

            const Shader& shader = assets_.shader(handle);
            layers_.refreshShader(handle, shader);

            scenes_.forEach([&](Scene& scene) {
                for (const auto& ent : scene.entities()) {
                    if (!ent)
                        continue;
                    ent->forEachComponent([&](Component& comp) {
                        if (auto* effect = dynamic_cast<HasShaderEffect*>(&comp)) {
                            if (effect->handle() == handle) {
                                effect->setShader(shader);
                            }
                        }
                    });
                }
            });
        });

        aspectRatio_ = cfg.aspectRatio > 0 ? cfg.aspectRatio : (cfg.width / cfg.height);
        resizeMode_ = cfg.resizeMode;
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

    void Runtime::requestPopScene() {
        postFrame([this] { popScene(); });
    }

    void Runtime::postFrame(std::function<void()> cb) {
        if (cb) {
            postFrameTasks_.push_back(std::move(cb));
        }
    }

    void Runtime::clearScenes() {
        scenes_.clear();
    }

    void Runtime::run() {
        running_ = true;
        while (running_ && !WindowShouldClose()) {
            renderer_.beginFrame();
            const float dt = GetFrameTime();

            assets_.update(dt);

            if (fullscreenKey_.has_value() && input_.keyPressed(*fullscreenKey_)) {
                window_.toggleFullscreen();
            }

            const auto w = static_cast<float>(GetRenderWidth());
            const auto h = static_cast<float>(GetRenderHeight());
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

            for (auto& view : views_) {
                Camera2DController* cam2d = view.camera2D();
                Camera3DController* cam3d = view.camera3D();
                if (cam2d) {
                    cam2d->update(dt);
                }
                if (cam3d) {
                    cam3d->update(dt);
                }

                BeginScissorMode(static_cast<int>(view.viewport.x), static_cast<int>(view.viewport.y),
                                 static_cast<int>(view.viewport.width), static_cast<int>(view.viewport.height));

                if (view.space == ViewSpace::World3D) {
                    if (cam3d) {
                        renderer_.flushPreparedWorld3D(cam3d->cam3d(), view.viewport, cam2d == nullptr);
                    }
                }

                if (cam2d) {
                    renderer_.flushPreparedWorld2D(cam2d->cam2d(), view.viewport);
                }

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
                assets_.debugOverlay();
                scenes_.drawDebug();
                rlImGuiEnd();
            }

            EndDrawing();

            flushPostFrame_();
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

    ViewId Runtime::addView(Camera2DController& camera, const Rectangle& viewport,
                            std::function<Rectangle(float width, float height)> onResize,
                            const std::optional<ResizeMode> mode,
                            const std::optional<float> aspectRatio) {
        const ViewId id = nextViewId_++;
        views_.push_back(View{
            id,
            std::ref(camera),
            std::nullopt,
            viewport,
            std::move(onResize),
            mode,
            aspectRatio,
            ViewSpace::World2D
        });
        return id;
    }

    ViewId Runtime::addView3D(Camera3DController& camera, const Rectangle& viewport,
                              std::function<Rectangle(float width, float height)> onResize,
                              const std::optional<ResizeMode> mode,
                              const std::optional<float> aspectRatio,
                              Camera2DController* overlay2D) {
        const ViewId id = nextViewId_++;
        std::optional<std::reference_wrapper<Camera2DController>> cam2dRef = std::nullopt;
        if (overlay2D) {
            cam2dRef = std::ref(*overlay2D);
        }
        views_.push_back(View{
            id,
            cam2dRef,
            std::ref(camera),
            viewport,
            std::move(onResize),
            mode,
            aspectRatio,
            ViewSpace::World3D
        });
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

    void Runtime::flushPostFrame_() {
        if (postFrameTasks_.empty())
            return;
        const auto tasks = std::move(postFrameTasks_);
        postFrameTasks_.clear();
        for (auto& task : tasks) {
            if (task) {
                task();
            }
        }
    }


} // namespace rlge
