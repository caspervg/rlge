#include "panel.hpp"

#include <algorithm>

namespace rlge::ui {

    Vector2 Panel::measureContent() const {
        auto maxSize = Vector2{0.0f, 0.0f};
        for (const auto& child : children_) {
            if (!child->visible())
                continue;

            const auto measured = child->measureContent();
            const auto& childLayout = child->layout();

            auto childWidth = childLayout.size.x > 0.0f
                ? childLayout.size.x
                : measured.x;
            auto childHeight = childLayout.size.y > 0.0f
                ? childLayout.size.y
                : measured.y;

            maxSize.x = std::max(maxSize.x, childWidth);
            maxSize.y = std::max(maxSize.y, childHeight);
        }

        maxSize.x += layout_.padding.x * 2.0f;
        maxSize.y += layout_.padding.y * 2.0f;

        if (layout_.size.x > 0.0f)
            maxSize.x = layout_.size.x;
        if (layout_.size.y > 0.0f)
            maxSize.y = layout_.size.y;

        return maxSize;
    }

    void Panel::draw(RenderQueue& rq) const {
        if (style_.background.a > 0 || (style_.border.a > 0 && style_.borderThickness > 0.0f)) {
            const auto rect = bounds();
            if (style_.background.a > 0) {
                rq.submitUI([rect, bg = style_.background] {
                    DrawRectangleV({rect.x, rect.y}, {rect.width, rect.height}, bg);
                });
            }
            if (style_.border.a > 0 && style_.borderThickness > 0.0f) {
                rq.submitUI([rect, border = style_.border, t = style_.borderThickness] {
                    DrawRectangleLinesEx(rect, t, border);
                });
            }
        }

        Widget::draw(rq);
    }

} // namespace rlge::ui
