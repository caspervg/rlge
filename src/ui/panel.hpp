#pragma once
#include <string>

#include "widget.hpp"

namespace rlge::ui {

    struct PanelStyle {
        Color background{0, 0, 0, 0};
        Color border{0, 0, 0, 0};
        float borderThickness = 0.0f;
    };

    class Panel : public Widget {
    public:
        explicit Panel(LayoutConfig layout = {}, PanelStyle style = {})
            : Widget(layout)
            , style_(style) {}

        Vector2 measureContent() const override;
        void draw(RenderQueue& rq) const override;

        void setStyle(const PanelStyle& style) { style_ = style; }
        [[nodiscard]] const PanelStyle& style() const { return style_; }
        Panel& style(const PanelStyle& s) { setStyle(s); return *this; }

    private:
        PanelStyle style_{};
    };

} // namespace rlge::ui
