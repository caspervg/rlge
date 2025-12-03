#include "lit_scene.hpp"
#include "runtime.hpp"

namespace rlge {

LitScene::LitScene(Runtime& r) :
    Scene(r), lighting_() {}

LitScene::~LitScene() {
    if (sceneBuffer_.texture.id != 0) {
        UnloadRenderTexture(sceneBuffer_);
    }
}

void LitScene::enter() {
    Scene::enter();

    // Initialize scene buffer and lighting system with current window size
    auto [width, height] = runtime().window().size();
    bufferSize_ = {width, height};
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);

    sceneBuffer_ = LoadRenderTexture(w, h);
    lighting_.init(w, h);

    // Set sensible ambient default (dark environment)
    lighting_.setAmbient({30, 30, 40, 255});
}

RenderTexture2D* LitScene::beginWorldRenderTarget() {
    ensureBuffersMatchWindow_();
    lighting_.beginFrame();
    return &sceneBuffer_;
}

void LitScene::afterWorldRender(RenderTexture2D* target, const std::vector<View>& views) {
    if (!target) {
        return;
    }

    // Render lights for each view into the shared light buffer
    for (const auto& view : views) {
        Camera& cam = view.camera.get();
        lighting_.renderLights(cam.cam2d(), view.viewport);
    }

    // Apply lighting to the scene buffer and present to the screen
    lighting_.applyLighting(target->texture);

    // Draw content that should remain unlit (overlays, debug, etc.)
    drawUnlit();
}

void LitScene::ensureBuffersMatchWindow_() {
    auto [width, height] = runtime().window().size();
    if (bufferSize_.x == width && bufferSize_.y == height) {
        return;
    }

    bufferSize_ = {width, height};
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);

    if (sceneBuffer_.texture.id != 0) {
        UnloadRenderTexture(sceneBuffer_);
    }
    sceneBuffer_ = LoadRenderTexture(w, h);

    lighting_.resize(w, h);
}

} // namespace rlge
