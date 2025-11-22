#include "raylib.h"
#include "render_entity.hpp"

namespace demo {
    class Label : public rlge::RenderEntity {
    public:
        Label(rlge::Scene& s, std::string text, Vector2 position, int fontSize = 20, Color color = RAYWHITE)
            : RenderEntity(s)
            , text_(std::move(text))
            , position_(position)
            , fontSize_(fontSize)
            , color_(color) {}

        void draw() override {
            const auto text = text_; // copy for capture-by-value
            const auto pos = position_;
            const int size = fontSize_;
            const Color col = color_;
            rq().submitUI([text, pos, size, col]() {
                DrawText(text.c_str(), static_cast<int>(pos.x), static_cast<int>(pos.y), size, col);
            });
        }

    private:
        std::string text_;
        Vector2 position_;
        int fontSize_;
        Color color_;
    };

    class Button : public rlge::RenderEntity {
    public:
        Button(rlge::Scene& s, Rectangle rect, std::string label, std::function<void()> onClick = {})
            : RenderEntity(s)
            , rect_(rect)
            , label_(std::move(label))
            , onClick_(std::move(onClick)) {}

        void update(float /*dt*/) override {
            const Vector2 mouse = GetMousePosition();
            hovered_ = CheckCollisionPointRec(mouse, rect_);

            if (hovered_ && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                pressed_ = true;
            }
            if (pressed_ && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                pressed_ = false;
                if (hovered_ && onClick_) {
                    onClick_();
                }
            }
        }

        void draw() override {
            const Rectangle rect = rect_;
            const bool hovered = hovered_;
            const bool pressed = pressed_;
            const std::string label = label_;
            rq().submitUI([rect, hovered, pressed, label]() {
                const Color base = DARKGRAY;
                const Color hoverCol = GRAY;
                const Color pressedCol = LIGHTGRAY;
                const Color col = pressed ? pressedCol : (hovered ? hoverCol : base);

                DrawRectangleRec(rect, col);
                DrawRectangleLinesEx(rect, 2.0f, BLACK);

                const int fontSize = 20;
                const int textWidth = MeasureText(label.c_str(), fontSize);
                const int textX = static_cast<int>(rect.x + (rect.width - textWidth) / 2.0f);
                const int textY = static_cast<int>(rect.y + (rect.height - fontSize) / 2.0f);
                DrawText(label.c_str(), textX, textY, fontSize, RAYWHITE);
            });
        }

    private:
        Rectangle rect_;
        std::string label_;
        std::function<void()> onClick_;
        bool hovered_{false};
        bool pressed_{false};
    };

    class Checkbox : public rlge::RenderEntity {
    public:
        Checkbox(rlge::Scene& s, Rectangle rect, std::string label, bool initial = false)
            : RenderEntity(s)
            , rect_(rect)
            , label_(std::move(label))
            , checked_(initial) {}

        [[nodiscard]] bool checked() const { return checked_; }

        void update(float /*dt*/) override {
            const Vector2 mouse = GetMousePosition();
            const bool inside = CheckCollisionPointRec(mouse, rect_);

            if (inside && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                checked_ = !checked_;
            }
        }

        void draw() override {
            const Rectangle box = rect_;
            const std::string label = label_;
            const bool checked = checked_;
            rq().submitUI([box, label, checked]() {
                DrawRectangleLinesEx(box, 2.0f, RAYWHITE);
                if (checked) {
                    DrawRectangle(static_cast<int>(box.x) + 4,
                                  static_cast<int>(box.y) + 4,
                                  static_cast<int>(box.width) - 8,
                                  static_cast<int>(box.height) - 8,
                                  RAYWHITE);
                }

                const int fontSize = 20;
                const int textX = static_cast<int>(box.x + box.width + 10.0f);
                const int textY = static_cast<int>(box.y + (box.height - fontSize) / 2.0f);
                DrawText(label.c_str(), textX, textY, fontSize, RAYWHITE);
            });
        }

    private:
        Rectangle rect_;
        std::string label_;
        bool checked_;
    };

    class Toggle : public rlge::RenderEntity {
    public:
        Toggle(rlge::Scene& s, Rectangle rect, bool initial = false)
            : RenderEntity(s)
            , rect_(rect)
            , on_(initial) {}

        [[nodiscard]] bool on() const { return on_; }

        void update(float /*dt*/) override {
            const Vector2 mouse = GetMousePosition();
            const bool inside = CheckCollisionPointRec(mouse, rect_);

            if (inside && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                on_ = !on_;
            }
        }

        void draw() override {
            const Rectangle rect = rect_;
            const bool on = on_;
            rq().submitUI([rect, on]() {
                const float radius = rect.height / 2.0f;
                const Color bg = on ? GREEN : DARKGRAY;

                DrawRectangleRounded(rect, 0.5f, 16, bg);
                const float knobX = on ? rect.x + rect.width - radius : rect.x + radius;
                const float knobY = rect.y + radius;
                DrawCircle(static_cast<int>(knobX), static_cast<int>(knobY),
                           radius - 3.0f, RAYWHITE);
            });
        }

    private:
        Rectangle rect_;
        bool on_;
    };
}