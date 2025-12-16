#include "camera.hpp"
#include "imgui.h"
#include "raylib.h"
#include "render_entity.hpp"
#include "runtime.hpp"
#include "window.hpp"
using namespace rlge;
namespace fs = std::filesystem;

struct FullscreenParams {
    float time = 0.0f;
    Vector2 resolution = {1920.0f, 1080.0f};
    Vector2 mousePos = {0.0f, 0.0f};
};

class FullscreenQuad final : public RenderEntity {
public:
    FullscreenQuad(Scene& scene, const LayerId layer, const Shader shader,
                   ShaderParams<FullscreenParams>* params) :
        RenderEntity(scene)
        , layer_(layer)
        , shader_(shader)
        , params_(params) {}

    void draw() override {
        auto* params = params_;
        rq().submitCustom(layer_, 0, shader_, [params, this] {
            if (params) {
                params->apply();
            }
            auto [x, y] = scene().runtime().window().size();
            DrawRectangle(0, 0, x, y, WHITE);
        });
    }

    void setShaderAndParams(const Shader shader, ShaderParams<FullscreenParams>* params) {
        shader_ = shader;
        params_ = params;
    }

private:
    LayerId layer_;
    Shader shader_;
    ShaderParams<FullscreenParams>* params_;
};

class FpsCounter final : public RenderEntity {
public:
    explicit FpsCounter(Scene& scene) :
        RenderEntity(scene) {}

    void draw() override {
        rq().submitUI([] {
            DrawFPS(10, 10);
        });
    }
};

class ShaderLearnScene final : public Scene, public HasDebugOverlay {
public:
    explicit ShaderLearnScene(Runtime& r) :
        Scene(r) {}

    void enter() override {
        assets().setRoot(findShaderDir_());
        assets().hotReload(true);

        assets().setShaderReloadCallback([this](ShaderHandle handle, const bool success) {
            if (success && handle == shaderHandle_) {
                rebindShaderParams_();
            }
        });

        const auto [w, h] = runtime().window().size();
        setSingleView(camera_);

        shaderHandle_ = assets().loadShader("fullscreen", "fullscreen.vert", "fullscreen.frag");
        auto& shader = assets().shader(shaderHandle_);

        shaderLayer_ = layers().create("fullscreen", 0, true);

        shaderParams_ = std::make_unique<ShaderParams<FullscreenParams>>(shader);
        shaderParams_->bind("u_time", &FullscreenParams::time);
        shaderParams_->bind("u_resolution", &FullscreenParams::resolution);
        shaderParams_->bind("u_mouse", &FullscreenParams::mousePos);

        quad_ = &spawn<FullscreenQuad>(shaderLayer_, shader, shaderParams_.get());
        spawn<FpsCounter>();
    }

    void update(const float dt) override {
        Scene::update(dt);

        elapsedTime_ += dt;
        updateShaderParams_();
    }

    void debugOverlay() override {
        ImGui::Begin("Shader Demo");
        ImGui::Separator();
        ImGui::End();
    }

private:
    void updateShaderParams_() {
        params_.time = elapsedTime_;
        params_.resolution = {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};

        auto [x, y] = GetMousePosition();
        params_.mousePos = {(x / params_.resolution.x) * 2.0f - 1.0f, 1.0f - (y / params_.resolution.y) * 2.0f};

        if (shaderParams_) {
            shaderParams_->params() = params_;
        }
    }

    void rebindShaderParams_() {
        auto& shader = assets().shader(shaderHandle_);

        shaderParams_ = std::make_unique<ShaderParams<FullscreenParams>>(shader);
        shaderParams_->bind("u_time", &FullscreenParams::time);
        shaderParams_->bind("u_resolution", &FullscreenParams::resolution);
        shaderParams_->bind("u_mouse", &FullscreenParams::mousePos);
        shaderParams_->params() = params_;

        if (quad_) {
            quad_->setShaderAndParams(shader, shaderParams_.get());
        }
    }

    static fs::path findShaderDir_() {
        const std::vector candidates = {
            fs::current_path() / "examples" / "shader_learn",
            fs::current_path(). parent_path() / "examples" / "shader_learn",
            fs::current_path(),
        };

        for (const auto& dir : candidates) {
            if (fs::exists(dir / "fullscreen.frag")) {
                return dir;
            }
        }

        return fs::current_path();
    }

private:
    LayerId shaderLayer_ = InvalidLayerId;
    ShaderHandle shaderHandle_{InvalidShaderHandle};
    std::unique_ptr<ShaderParams<FullscreenParams>> shaderParams_;
    FullscreenQuad* quad_ = nullptr;
    FullscreenParams params_;
    float elapsedTime_ = 0.0f;
    rlge::Camera2DController camera_;
};

auto main() -> int {
    Runtime runtime(WindowConfig{
        .width = 800,
        .height = 600,
        .fps = 240,
        .title = "RLGE Shader Demo",
        .debugKey = KeyCode::F11
    });

    runtime.pushScene<ShaderLearnScene>();
    runtime.run();

    return 0;
}
