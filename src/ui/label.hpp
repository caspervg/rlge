#pragma once
#include <functional>
#include <string>

#include "widget.hpp"

namespace rlge::ui {

    using TextProvider = std::function<std::string()>;

    template <typename Fn>
    TextProvider bind(Fn&& fn) {
        return [fn = std::forward<Fn>(fn)]() { return fn(); };
    }

    struct LabelStyle {
        Color color{255, 255, 255, 255};
        float fontSize = 16.0f;
        float spacing = 0.0f;
    };

    class Label : public Widget {
    public:
        Label(std::string text, LayoutConfig layout = {}, LabelStyle style = {}, const Font* font = nullptr)
            : Widget(layout)
            , text_(std::move(text))
            , style_(style)
            , font_(font) {}
        Label(TextProvider provider, LayoutConfig layout = {}, LabelStyle style = {}, const Font* font = nullptr)
            : Widget(layout)
            , style_(style)
            , font_(font)
            , provider_(std::move(provider)) {}

        Vector2 measureContent() const override;
        void draw(RenderQueue& rq) const override;

        void setText(std::string text) { text_ = std::move(text); invalidateLayout(); }
        [[nodiscard]] const std::string& text() const { return text_; }

        void setTextProvider(TextProvider provider) { provider_ = std::move(provider); invalidateLayout(); }
        [[nodiscard]] const TextProvider& textProvider() const { return provider_; }

        void setStyle(const LabelStyle& style) { style_ = style; invalidateLayout(); }
        [[nodiscard]] const LabelStyle& style() const { return style_; }

        void setFont(const Font* font) { font_ = font; invalidateLayout(); }
        [[nodiscard]] const Font* font() const { return font_; }

        // Builder helpers
        Label& text(std::string t) { setText(std::move(t)); return *this; }
        Label& provider(TextProvider p) { setTextProvider(std::move(p)); return *this; }
        Label& style(const LabelStyle& s) { setStyle(s); return *this; }
        Label& font(const Font* f) { setFont(f); return *this; }

    private:
        const Font& resolvedFont() const;
        std::string currentText() const;

        std::string text_{};
        LabelStyle style_{};
        const Font* font_{nullptr};
        TextProvider provider_{};
    };

} // namespace rlge::ui
