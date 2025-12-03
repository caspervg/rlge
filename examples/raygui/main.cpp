#include <algorithm>
#include <cmath>

#include "raygui.h"
#include "raylib.h"

#include "render_entity.hpp"
#include "runtime.hpp"
#include "transformer.hpp"
#include "window.hpp"
#include "util/raygui_helpers.hpp"

using namespace rlge;

class MovingBox final : public RenderEntity {
public:
    explicit MovingBox(Scene& scene) : RenderEntity(scene) {
        auto tr = add<rlge::Transform>();
        tr.position = {200.0f, 200.0f};
    }

    void update(const float dt) override {
        RenderEntity::update(dt);
        if (!moving_) return;

        auto* tr = get<rlge::Transform>();
        if (!tr) return;

        const auto [x, y] = scene().runtime().window().size();
        tr->position.x += std::cos(time_) * speed_ * dt;
        tr->position.y += std::sin(time_ * 0.7f) * speed_ * dt;
        time_ += dt;

        // Keep on screen
        tr->position.x = std::clamp(tr->position.x, 50.0f, x - 50.0f);
        tr->position.y = std::clamp(tr->position.y, 50.0f, y - 50.0f);
    }

    void draw() override {
        const auto* tr = get<rlge::Transform>();
        if (!tr) return;

        rq().submitWorld([pos = tr->position, c = color_] {
            DrawRectangleV(Vector2{pos.x - 40.0f, pos.y - 40.0f}, Vector2{80.0f, 80.0f}, c);
        });
    }

    Color color_{RED};
    float speed_{180.0f};
    bool moving_{true};
    float hue_{0.0f};

private:
    float time_{0.0f};
};

class UiPanel final : public RenderEntity {
public:
    UiPanel(Scene& scene, MovingBox& box) : RenderEntity(scene), box_(box) {}

    void draw() override {
        rq().submitUI([this] {
            GuiSetStyle(DEFAULT, TEXT_SIZE, 18);
            GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            GuiSetState(STATE_NORMAL);

            const Rectangle panel{20, 20, 300, 210};
            GuiPanel(panel, "raygui demo");

            const Rectangle slider{30, 70, 240, 24};
            GuiLabel({30, 50, 120, 16}, "Speed");
            GuiSlider(slider, nullptr, nullptr, &box_.speed_, 50.0f, 400.0f);

            GuiLabel({30, 105, 120, 16}, "Box Color");
            const Rectangle colorPicker{30, 125, 240, 24};
            GuiSliderBar(colorPicker, nullptr, nullptr, &box_.hue_, 0.0f, 360.0f);
            box_.color_ = ColorFromHSV(box_.hue_, 0.75f, 0.9f);

            const Rectangle toggle{30, 160, 20, 20};
            GuiCheckBox(toggle, " Animate", &box_.moving_);
        });
    }

private:
    MovingBox& box_;
};

class RayguiDemoScene final : public Scene {
public:
    explicit RayguiDemoScene(Runtime& r) : Scene(r) {}

    void enter() override {
        camera_ = rlge::Camera();
        setSingleView(camera_);

        auto& box = spawn<MovingBox>();
        ui_ = &spawn<UiPanel>(box);

        // Load an optional style (exported from rGuiStyler) if present.
        // Not fatal if missing; this keeps the demo self-contained.
        ui::loadStyle("../examples/raygui/style.rgs");
        ui::applyBaseStyle(18);
        GuiSetStyle(CHECKBOX, TEXT_ALIGNMENT, TEXT_ALIGN_RIGHT); // keep checkbox text to the right of the box
    }

private:
    rlge::Camera camera_;
    UiPanel* ui_{nullptr};
};

int main() {
    Runtime runtime(WindowConfig{
        .width = 960,
        .height = 540,
        .fps = 60,
        .title = "RLGE raygui demo"
    });

    runtime.pushScene<RayguiDemoScene>();
    runtime.run();
    return 0;
}
