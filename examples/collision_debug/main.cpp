#include "runtime.hpp"
#include "window.hpp"
#include "debug.hpp"
#include "imgui.h"

#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"
#include "box2d_physics.hpp"

using namespace rlge;

namespace {
    std::vector<Vector2> ensureCCWWinding(std::vector<Vector2> pts) {
        const size_t n = pts.size();
        if (n < 3)
            return pts;

        float area = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            const auto& p0 = pts[i];
            const auto& p1 = pts[(i + 1) % n];
            area += p0.x * p1.y - p1.x * p0.y;
        }

        // In screen coordinates (+Y down), positive area corresponds to clockwise winding.
        // DrawTriangle expects counter-clockwise, so reverse when area > 0.
        if (area > 0.0f) {
            std::reverse(pts.begin(), pts.end());
        }

        return pts;
    }
}

class PlayerEntity final : public RenderEntity {
public:
    explicit PlayerEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {200.0f, 200.0f};

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_dynamicBody,
            .gravityScale = 0.0f,
            .linearDamping = 5.0f,
            .fixedRotation = true
        };
        body_ = &add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.3f,
            .restitution = 0.0f,
            .isSensor = false,
            .layer = ColliderLayerMask::LAYER_PLAYER,
            .mask = ColliderLayerMask::LAYER_WORLD
        };
        body_->addBoxFixture(32.0f, 32.0f, fixtureCfg);
    }

    void update(const float dt) override {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        const auto& in = scene().input();
        constexpr auto speed = 300.0f;

        Vector2 force = {0.0f, 0.0f};
        if (in.down(Action::MoveLeft))
            force.x -= speed;
        if (in.down(Action::MoveRight))
            force.x += speed;
        if (in.down(Action::MoveUp))
            force.y -= speed;
        if (in.down(Action::MoveDown))
            force.y += speed;

        if (force.x != 0.0f || force.y != 0.0f) {
            body_->applyForceToCenter(force);
        }
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<Transform>();
            if (!tr)
                return;
            DrawRectangle(
                static_cast<int>(tr->position.x - 16.0f),
                static_cast<int>(tr->position.y - 16.0f),
                32, 32,
                Color{100, 200, 255, 255});
        });
    }

private:
    Box2DBody* body_{nullptr};
};

class StaticCircleEntity final : public RenderEntity {
public:
    explicit StaticCircleEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {350.0f, 200.0f};

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_staticBody,
            .gravityScale = 0.0f
        };
        auto& body = add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.3f,
            .restitution = 0.0f,
            .isSensor = false,
            .layer = ColliderLayerMask::LAYER_WORLD,
            .mask = ColliderLayerMask::LAYER_PLAYER
        };
        body.addCircleFixture(24.0f, {0.0f, 0.0f}, fixtureCfg);
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<Transform>();
            if (!tr)
                return;
            DrawCircleV(tr->position, 24.0f, Color{255, 200, 120, 255});
        });
    }
};

class StaticBoxEntity final : public RenderEntity {
public:
    explicit StaticBoxEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {200.0f, 300.0f};
        tr.rotation = 15.0f; // rotation for visual effect

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_staticBody,
            .gravityScale = 0.0f
        };
        auto& body = add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.3f,
            .restitution = 0.0f,
            .isSensor = false,
            .layer = ColliderLayerMask::LAYER_WORLD,
            .mask = ColliderLayerMask::LAYER_PLAYER
        };
        body.addBoxFixture(80.0f, 20.0f, fixtureCfg);
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<Transform>();
            if (!tr)
                return;

            // Draw as rotated rectangle
            const float angle = tr->rotation * DEG2RAD;
            const float halfW = 40.0f;
            const float halfH = 10.0f;
            
            Vector2 corners[4] = {
                {-halfW, -halfH},
                { halfW, -halfH},
                { halfW,  halfH},
                {-halfW,  halfH}
            };
            
            // Rotate and translate corners
            for (int i = 0; i < 4; ++i) {
                float x = corners[i].x * cosf(angle) - corners[i].y * sinf(angle);
                float y = corners[i].x * sinf(angle) + corners[i].y * cosf(angle);
                corners[i] = {x + tr->position.x, y + tr->position.y};
            }
            
            constexpr Color c{180, 100, 255, 255};
            DrawTriangle(corners[0], corners[1], corners[2], c);
            DrawTriangle(corners[0], corners[2], corners[3], c);
        });
    }
};

class StaticPolygonEntity final : public RenderEntity {
public:
    explicit StaticPolygonEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {450.0f, 260.0f};

        std::vector<Vector2> localPoints{
            {-30.0f, -20.0f},
            { 40.0f, -10.0f},
            { 50.0f,  25.0f},
            {  0.0f,  40.0f},
            {-35.0f,  10.0f}
        };

        Box2DBodyConfig bodyCfg = {
            .bodyType = b2_staticBody,
            .gravityScale = 0.0f
        };
        auto& body = add<Box2DBody>(scene().physics(), bodyCfg);

        Box2DFixtureConfig fixtureCfg = {
            .density = 1.0f,
            .friction = 0.3f,
            .restitution = 0.0f,
            .isSensor = false,
            .layer = ColliderLayerMask::LAYER_WORLD,
            .mask = ColliderLayerMask::LAYER_PLAYER
        };
        body.addPolygonFixture(localPoints, fixtureCfg);
        
        localPoints_ = localPoints;
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* tr = get<Transform>();
            if (!tr)
                return;

            // Transform local points to world space
            std::vector<Vector2> worldPoints;
            for (const auto& p : localPoints_) {
                worldPoints.push_back({p.x + tr->position.x, p.y + tr->position.y});
            }
            
            auto pts = ensureCCWWinding(worldPoints);
            const size_t n = pts.size();
            if (n < 3)
                return;

            const Color c{120, 255, 160, 255};
            // Simple triangle fan fill
            for (size_t i = 1; i + 1 < n; ++i) {
                DrawTriangle(pts[0], pts[i], pts[i + 1], c);
            }
        });
    }

private:
    std::vector<Vector2> localPoints_;
};

class CollisionDemoScene final : public Scene, public HasDebugOverlay {
public:
    explicit CollisionDemoScene(Runtime& r) :
        Scene(r) {}

    void enter() override {
        camera_ = rlge::Camera();
        setSingleView(camera_);

        player_ = &spawn<PlayerEntity>();
        spawn<StaticCircleEntity>();
        spawn<StaticBoxEntity>();
        spawn<StaticPolygonEntity>();
    }

    void update(const float dt) override {
        Scene::update(dt);
    }

    void debugOverlay() override {
        ImGui::Begin("Collision Demo");
        ImGui::Text("Use WASD to move the box.");
        ImGui::Text("Press F1 to toggle this UI.");
        ImGui::Text("Enable debug drawing in the 'Box2D Physics' window");
        ImGui::Text("to see Box2D fixtures and shapes.");
        ImGui::End();
    }

private:
    PlayerEntity* player_{nullptr};
    rlge::Camera camera_;
};

int main() {
    WindowConfig cfg{
        .width = 800,
        .height = 450,
        .fps = 60,
        .title = "RLGE Collision Demo (Box2D)"
    };

    Runtime runtime(cfg);

    runtime.input().bind(Action::MoveLeft, KeyCode::A);
    runtime.input().bind(Action::MoveRight, KeyCode::D);
    runtime.input().bind(Action::MoveUp, KeyCode::W);
    runtime.input().bind(Action::MoveDown, KeyCode::S);

    runtime.pushScene<CollisionDemoScene>();
    runtime.run();

    return 0;
}
