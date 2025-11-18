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
            auto& tiles = assets().loadTexture("tiles", "../examples/tilemap_demo/assets/tiles.png");
            tilemap_ = &rlge::Tilemap::loadTMX(*this, tiles, "../examples/tilemap_demo/assets/map.tmj");
            tilemap_->get<rlge::Transform>()->position = {40, 20};
        }

        void draw() override {
            Scene::draw();
            DrawText("Arrow keys move the map", 10, 10, 20, RAYWHITE);
        }

        void update(float dt) override {
            Scene::update(dt);
            if (tilemap_) {
                auto* tr = tilemap_->get<rlge::Transform>();
                if (IsKeyDown(KEY_LEFT)) tr->position.x -= 200 * dt;
                if (IsKeyDown(KEY_RIGHT)) tr->position.x += 200 * dt;
                if (IsKeyDown(KEY_UP)) tr->position.y -= 200 * dt;
                if (IsKeyDown(KEY_DOWN)) tr->position.y += 200 * dt;
            }
        }

    private:
        rlge::Tilemap* tilemap_ = nullptr;
    };
}

int main() {
    rlge::WindowConfig cfg{
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
