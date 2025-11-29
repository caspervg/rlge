#include "label.hpp"

namespace rlge::ui {

    const Font& Label::resolvedFont() const {
        if (font_) {
            return *font_;
        }
        static const Font kDefault = GetFontDefault();
        return kDefault;
    }

    std::string Label::currentText() const {
        if (provider_) {
            return provider_();
        }
        return text_;
    }

    Vector2 Label::measureContent() const {
        const auto& font = resolvedFont();
        const auto text = currentText();
        const auto size = MeasureTextEx(font, text.c_str(), style_.fontSize, style_.spacing);
        auto result = size;
        result.x += layout_.padding.x * 2.0f;
        result.y += layout_.padding.y * 2.0f;

        if (layout_.size.x > 0.0f)
            result.x = layout_.size.x;
        if (layout_.size.y > 0.0f)
            result.y = layout_.size.y;

        return result;
    }

    void Label::draw(RenderQueue& rq) const {
        const auto rect = bounds();
        const Vector2 pos{rect.x + layout_.padding.x, rect.y + layout_.padding.y};
        const auto& font = resolvedFont();
        const auto color = style_.color;
        const auto text = currentText();

        rq.submitUI([pos, font, color, text, style = style_] {
            DrawTextEx(font, text.c_str(), pos, style.fontSize, style.spacing, color);
        });

        Widget::draw(rq);
    }

} // namespace rlge::ui
