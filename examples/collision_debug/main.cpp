#include "runtime.hpp"
#include "window.hpp"
#include "debug.hpp"
#include "imgui.h"

#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"
#include "collision/collision_system.hpp"
#include "collision/shape/box_collider.hpp"
#include "collision/shape/circle_collider.hpp"
#include "collision/shape/obb_collider.hpp"
#include "collision/shape/polygon_collider.hpp"

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

        // Simple local box around the origin of the entity
        constexpr Rectangle local{-16.0f, -16.0f, 32.0f, 32.0f};

        auto& sys = scene().runtime().services().collisions();
        add<BoxCollider>(
            sys,
            ColliderType::Solid,
            ColliderLayerMask::LAYER_PLAYER,
            ColliderLayerMask::LAYER_WORLD,
            local,
            false);
    }

    void update(float dt) override {
        RenderEntity::update(dt);

        auto* tr = get<rlge::Transform>();
        if (!tr)
            return;

        const auto& in = scene().input();
        constexpr auto speed = 150.0f;

        if (in.down("left"))
            tr->position.x -= speed * dt;
        if (in.down("right"))
            tr->position.x += speed * dt;
        if (in.down("up"))
            tr->position.y -= speed * dt;
        if (in.down("down"))
            tr->position.y += speed * dt;
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            auto* col = get<BoxCollider>();
            if (!col)
                return;
            const Rectangle r = col->axisAlignedWorldBounds();
            DrawRectangleRec(r, Color{100, 200, 255, 255});
        });
    }
};

class StaticCircleEntity final : public RenderEntity {
public:
    explicit StaticCircleEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {350.0f, 200.0f};

        auto& sys = scene().runtime().services().collisions();
        const Vector2 localCenter{0.0f, 0.0f};
        constexpr float radius = 24.0f;
        add<CircleCollider>(
            sys,
            ColliderType::Solid,
            ColliderLayerMask::LAYER_WORLD,
            ColliderLayerMask::LAYER_PLAYER,
            localCenter,
            radius,
            false);
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            auto* col = get<CircleCollider>();
            if (!col)
                return;
            const Vector2 c = col->center();
            const float r = col->radius();
            DrawCircleV(c, r, Color{255, 200, 120, 255});
        });
    }
};

class StaticBoxEntity final : public RenderEntity {
public:
    explicit StaticBoxEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {200.0f, 300.0f};
        tr.rotation = 0.25f; // just to see rotation vs. AABB

        const Rectangle local{-40.0f, -10.0f, 80.0f, 20.0f};

        auto& sys = scene().runtime().services().collisions();
        add<BoxCollider>(
            sys,
            ColliderType::Solid,
            ColliderLayerMask::LAYER_WORLD,
            ColliderLayerMask::LAYER_PLAYER,
            local,
            false);
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            const auto* col = get<BoxCollider>();
            if (!col)
                return;

            auto pts = ensureCCWWinding(col->points()); // world-space, transformed corners
            if (pts.size() < 4)
                return;

            constexpr Color c{180, 100, 255, 255};
            // Draw as two filled triangles so rotation is respected
            DrawTriangle(pts[0], pts[1], pts[2], c);
            DrawTriangle(pts[0], pts[2], pts[3], c);
        });
    }
};

class StaticPolygonEntity final : public RenderEntity {
public:
    explicit StaticPolygonEntity(Scene& s)
        : RenderEntity(s) {
        auto& tr = add<rlge::Transform>();
        tr.position = {450.0f, 260.0f};

        auto& sys = scene().runtime().services().collisions();
        std::vector<Vector2> localPoints{
            {-30.0f, -20.0f},
            { 40.0f, -10.0f},
            { 50.0f,  25.0f},
            {  0.0f,  40.0f},
            {-35.0f,  10.0f}
        };

        add<PolygonCollider>(
            sys,
            ColliderType::Kinematic,
            ColliderLayerMask::LAYER_WORLD,
            ColliderLayerMask::LAYER_PLAYER,
            std::move(localPoints),
            false);
    }

    void draw() override {
        RenderEntity::draw();

        rq().submitWorld([this] {
            auto* col = get<PolygonCollider>();
            if (!col)
                return;

            auto pts = ensureCCWWinding(col->points()); // world-space polygon
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
        ImGui::Text("Enable 'Draw colliders' in the Collisions window");
        ImGui::Text("to see collider shapes and AABBs.");
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
        .title = "RLGE Collision Demo"
    };

    Runtime runtime(cfg);

    runtime.input().bind("left", KEY_A);
    runtime.input().bind("right", KEY_D);
    runtime.input().bind("up", KEY_W);
    runtime.input().bind("down", KEY_S);

    runtime.pushScene<CollisionDemoScene>();
    runtime.run();

    return 0;
}
