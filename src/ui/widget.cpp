#include "widget.hpp"

#include <algorithm>

namespace rlge::ui {

    void Widget::computeLayout(const Rectangle& parentBounds) {
        const auto width = layout_.size.x > 0.0f ? layout_.size.x : parentBounds.width;
        const auto height = layout_.size.y > 0.0f ? layout_.size.y : parentBounds.height;
        computedBounds_ = {parentBounds.x, parentBounds.y, width, height};
        layoutDirty_ = false;

        const auto content = contentBounds();
        // Lay out children with the full content area by default.
        for (auto& child : children_) {
            if (child->visible()) {
                child->computeLayout(content);
            }
        }
    }

    Rectangle Widget::contentBounds() const {
        const auto padX = layout_.padding.x;
        const auto padY = layout_.padding.y;
        const auto w = std::max(0.0f, computedBounds_.width - padX * 2.0f);
        const auto h = std::max(0.0f, computedBounds_.height - padY * 2.0f);
        return {computedBounds_.x + padX, computedBounds_.y + padY, w, h};
    }

    void Widget::draw(RenderQueue& rq) const {
        for (const auto& child : children_) {
            if (child->visible()) {
                child->draw(rq);
            }
        }
    }

} // namespace rlge::ui
