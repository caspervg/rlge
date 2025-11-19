#include "raylib.h"
#include "runtime.hpp"
#include "scene.hpp"
#include "tilemap.hpp"
#include "transformer.hpp"

namespace demo {
    class MultiViewScene : public rlge::Scene {
    public:
        using Scene::Scene;

        void enter() override {
            auto& tiles = assets().loadTexture("tiles", "../examples/tilemap/assets/tiles.png");
            tilemap_ = &rlge::Tilemap::loadTMX(*this, tiles, "../examples/tilemap/assets/map.tmj");
            tilemap_->get<rlge::Transform>()->position = {0, 0};

            // Configure three views: left, right, and a centered minimap at the bottom
            applyViewLayout();
        }

        void update(const float dt) override {
            Scene::update(dt);

            constexpr auto speed = 200.0f;

            // Left view controlled by arrow keys
            Vector2 leftDelta{0.0f, 0.0f};
            if (IsKeyDown(KEY_LEFT))
                leftDelta.x -= speed * dt;
            if (IsKeyDown(KEY_RIGHT))
                leftDelta.x += speed * dt;
            if (IsKeyDown(KEY_UP))
                leftDelta.y -= speed * dt;
            if (IsKeyDown(KEY_DOWN))
                leftDelta.y += speed * dt;

            leftCamera_.pan(leftDelta);

            // Right view controlled by WASD
            Vector2 rightDelta{0.0f, 0.0f};
            if (IsKeyDown(KEY_A))
                rightDelta.x -= speed * dt;
            if (IsKeyDown(KEY_D))
                rightDelta.x += speed * dt;
            if (IsKeyDown(KEY_W))
                rightDelta.y -= speed * dt;
            if (IsKeyDown(KEY_S))
                rightDelta.y += speed * dt;

            rightCamera_.pan(rightDelta);
        }

        void draw() override {
            Scene::draw();

            rq().submitUI([] {
                DrawText("Arrows: left view  |  WASD: right view",
                         10, 10, 20, RAYWHITE);
                DrawText("Minimap: full map overview",
                         10, 35, 18, RAYWHITE);
            });
        }

        void resume() override {
            applyViewLayout();
        }

        void pause() override {
            runtime().clearViews();
        }

        void exit() override {
            runtime().clearViews();
        }

    private:
        void applyViewLayout() {
            const Vector2 size = runtime().window().size();
            const float w = size.x;
            const float h = size.y;

            // Center each camera within its viewport
            leftCamera_.setOffset({w / 4.0f, h / 2.0f});
            leftCamera_.setTarget({0, 0});

            rightCamera_.setOffset({3.0f * w / 4.0f, h / 2.0f});
            rightCamera_.setTarget({0, 0});

            const float miniW = w / 3.0f;
            const float miniH = h / 3.0f;
            minimapCamera_.setOffset({w / 2.0f, h - miniH / 2.0f});

            // Configure minimap to show the entire tilemap and remain static
            const auto mapWorldWidth = static_cast<float>(tilemap_->mapWidth() * tilemap_->tileWidth());
            const auto mapWorldHeight = static_cast<float>(tilemap_->mapHeight() * tilemap_->tileHeight());
            const Vector2 mapCenter{mapWorldWidth / 2.0f, mapWorldHeight / 2.0f};
            minimapCamera_.setTarget(mapCenter);

            const float zoomX = miniW / mapWorldWidth;
            const float zoomY = miniH / mapWorldHeight;
            const float zoom = 0.9f * (zoomX < zoomY ? zoomX : zoomY);
            minimapCamera_.setZoom(zoom);

            // Replace the default full-screen view with our custom layout
            runtime().clearViews();

            // Left split-screen view
            runtime().addView(leftCamera_, Rectangle{0.0f, 0.0f, w / 2.0f, h});

            // Right split-screen view
            runtime().addView(rightCamera_, Rectangle{w / 2.0f, 0.0f, w / 2.0f, h});

            // Minimap at the bottom center
            runtime().addView(minimapCamera_,
                              Rectangle{(w - miniW) / 2.0f, h - miniH, miniW, miniH});
        }

    private:
        rlge::Tilemap* tilemap_ = nullptr;
        rlge::Camera leftCamera_;
        rlge::Camera rightCamera_;
        rlge::Camera minimapCamera_;
    };
}

int main() {
    const rlge::WindowConfig cfg{
        .width = 960,
        .height = 540,
        .fps = 60,
        .title = "Multi-View Camera Demo"
    };
    rlge::Runtime runtime(cfg);
    runtime.pushScene<demo::MultiViewScene>();
    runtime.run();
    return 0;
}
