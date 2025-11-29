#include "button.hpp"

namespace rlge::ui {

    const Font& Button::resolvedFont() const {
        if (font_) {
            return *font_;
        }
        static const Font kDefault = GetFontDefault();
        return kDefault;
    }

    Color Button::currentBackground() const {
        switch (state_) {
        case ButtonState::Hovered:
            return style_.hover;
        case ButtonState::Pressed:
            return style_.pressed;
        case ButtonState::Disabled:
            return style_.disabled;
        case ButtonState::Normal:
        default:
            return style_.background;
        }
    }

    std::string Button::currentText() const {
        if (provider_) {
            return provider_();
        }
        return text_;
    }

    Vector2 Button::measureContent() const {
        const auto& font = resolvedFont();
        const auto text = currentText();
        const auto textSize = MeasureTextEx(font, text.c_str(), style_.fontSize, style_.spacing);
        auto result = textSize;
        result.x += layout_.padding.x * 2.0f;
        result.y += layout_.padding.y * 2.0f;

        if (layout_.size.x > 0.0f)
            result.x = layout_.size.x;
        if (layout_.size.y > 0.0f)
            result.y = layout_.size.y;

        return result;
    }

    void Button::draw(RenderQueue& rq) const {
        const auto rect = bounds();
        const auto bg = currentBackground();
        const auto textColor = (state_ == ButtonState::Disabled) ? Color{style_.text.r, style_.text.g, style_.text.b, static_cast<unsigned char>(style_.text.a * 0.5f)} : style_.text;
        const auto& font = resolvedFont();
        const auto text = currentText();
        const Vector2 textPos{rect.x + layout_.padding.x, rect.y + layout_.padding.y};

        rq.submitUI([rect, bg] {
            DrawRectangleV({rect.x, rect.y}, {rect.width, rect.height}, bg);
        });

        rq.submitUI([textPos, font, text, style = style_, textColor] {
            DrawTextEx(font, text.c_str(), textPos, style.fontSize, style.spacing, textColor);
        });

        Widget::draw(rq);
    }

} // namespace rlge::ui
