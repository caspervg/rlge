#include "raylib.h"
#include "runtime.hpp"
#include "scene.hpp"
#include "tilemap.hpp"
#include "transformer.hpp"

namespace demo {
    class TilemapScene : public rlge::Scene {
    public:
        using Scene::Scene;

        void enter() override {
            auto& tiles = assets().loadTexture("tiles", "../examples/tilemap/assets/tiles.png");
            tilemap_ = &rlge::Tilemap::loadTMX(*this, tiles, "../examples/tilemap/assets/map.tmj");
            tilemap_->get<rlge::Transform>()->position = {0, 0};
        }

        void draw() override {
            Scene::draw();

            rq().submitUI([] {
                DrawText("Arrow keys move the map", 10, 10, 20, RAYWHITE);
            });
        }

        void update(float dt) override {
            Scene::update(dt);
            constexpr auto speed = 200.0f;
            Vector2 delta = {0, 0};
            if (IsKeyDown(KEY_LEFT))
                delta.x -= speed * dt;
            if (IsKeyDown(KEY_RIGHT))
                delta.x += speed * dt;
            if (IsKeyDown(KEY_UP))
                delta.y -= speed * dt;
            if (IsKeyDown(KEY_DOWN))
                delta.y += speed * dt;
            camera().pan(delta);
        }

    private:
        rlge::Tilemap* tilemap_ = nullptr;
    };
}

int main() {
    const rlge::WindowConfig cfg{
        .width = 640,
        .height = 360,
        .fps = 60,
        .title = "Tilemap Demo"
    };
    rlge::Runtime runtime(cfg);
    runtime.pushScene<demo::TilemapScene>();
    runtime.run();
    return 0;
}
