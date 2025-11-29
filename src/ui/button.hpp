#pragma once
#include <functional>
#include <string>

#include "label.hpp"
#include "widget.hpp"

namespace rlge::ui {

    enum class ButtonState {
        Normal,
        Hovered,
        Pressed,
        Disabled
    };

    struct ButtonStyle {
        Color background{60, 60, 80, 255};
        Color hover{80, 80, 110, 255};
        Color pressed{50, 50, 70, 255};
        Color disabled{40, 40, 40, 200};
        Color text{255, 255, 255, 255};
        float fontSize = 18.0f;
        float spacing = 0.0f;
        float borderRadius = 0.0f;
        Color border{0, 0, 0, 0};
        float borderThickness = 0.0f;
    };

    class Button : public Widget {
    public:
        Button(std::string text, const LayoutConfig& layout = {}, const ButtonStyle& style = {}, const Font* font = nullptr)
            : Widget(layout)
            , text_(std::move(text))
            , style_(style)
            , font_(font) {}

        [[nodiscard]] Vector2 measureContent() const override;
        void draw(RenderQueue& rq) const override;

        void setText(std::string text) { text_ = std::move(text); invalidateLayout(); }
        [[nodiscard]] const std::string& text() const { return text_; }

        void setStyle(const ButtonStyle& style) { style_ = style; invalidateLayout(); }
        [[nodiscard]] const ButtonStyle& style() const { return style_; }

        void setFont(const Font* font) { font_ = font; invalidateLayout(); }
        [[nodiscard]] const Font* font() const { return font_; }

        void setState(ButtonState state) { state_ = state; }
        [[nodiscard]] ButtonState state() const { return state_; }

        void setTextProvider(TextProvider provider) { provider_ = std::move(provider); invalidateLayout(); }
        [[nodiscard]] const TextProvider& textProvider() const { return provider_; }

        void setOnClick(std::function<void()> cb) { onClick_ = std::move(cb); }
        [[nodiscard]] const std::function<void()>& onClick() const { return onClick_; }
        [[nodiscard]] bool hasOnClick() const { return static_cast<bool>(onClick_); }

        // Builder helpers
        Button& text(std::string t) { setText(std::move(t)); return *this; }
        Button& provider(TextProvider p) { setTextProvider(std::move(p)); return *this; }
        Button& style(const ButtonStyle& s) { setStyle(s); return *this; }
        Button& font(const Font* f) { setFont(f); return *this; }
        Button& onClick(std::function<void()> cb) { setOnClick(std::move(cb)); return *this; }

    private:
        [[nodiscard]] const Font& resolvedFont() const;
        [[nodiscard]] Color currentBackground() const;
        [[nodiscard]] std::string currentText() const;

        std::string text_{};
        ButtonStyle style_{};
        const Font* font_{nullptr};
        ButtonState state_{ButtonState::Normal};
        TextProvider provider_{};
        std::function<void()> onClick_{};
    };

} // namespace rlge::ui
