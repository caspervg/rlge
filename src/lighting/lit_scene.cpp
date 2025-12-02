#include "lit_scene.hpp"
#include "runtime.hpp"
#include "entity.hpp"

namespace rlge {

LitScene::LitScene(Runtime& r)
    : Scene(r) {}

LitScene::~LitScene() {
    if (sceneBuffer_.texture.id != 0) {
        UnloadRenderTexture(sceneBuffer_);
    }
}

void LitScene::enter() {
    Scene::enter();

    // Initialize scene buffer and lighting system with current window size
    auto [width, height] = runtime().window().size();
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);

    sceneBuffer_ = LoadRenderTexture(w, h);
    lighting_.init(w, h);

    // Set sensible ambient default (dark environment)
    lighting_.setAmbient({30, 30, 40, 255});
}

void LitScene::draw() {
    // 1. Begin lighting frame (clears light buffer)
    lighting_.beginFrame();

    // 2. Render scene entities to sceneBuffer_
    BeginTextureMode(sceneBuffer_);
    ClearBackground(BLACK);

    // Draw all entities to the scene buffer
    for (const auto& e : entities()) {
        e->draw();
    }

    // 3. Flush world layers per view to sceneBuffer_
    rq().prepareWorld();

    for (const auto& view : views()) {
        if (!view.camera)
            continue;

        BeginScissorMode(
            static_cast<int>(view.viewport.x),
            static_cast<int>(view.viewport.y),
            static_cast<int>(view.viewport.width),
            static_cast<int>(view.viewport.height)
        );

        rq().flushPreparedWorld(view.camera->cam2d(), view.viewport);

        EndScissorMode();
    }

    EndTextureMode();

    // 4. Render lights for each view
    for (const auto& view : views()) {
        if (!view.camera)
            continue;
        lighting_.renderLights(view.camera->cam2d());
    }

    // 5. Apply lighting to scene and render to screen
    lighting_.applyLighting(sceneBuffer_.texture);

    // 6. Call drawUnlit() for content not affected by lighting
    drawUnlit();

    // 7. Flush UI
    rq().flushUI();
}

} // namespace rlge
