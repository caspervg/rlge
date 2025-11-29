#pragma once

#include "widget.hpp"

namespace rlge::ui {

    class Spacer : public Widget {
    public:
        explicit Spacer(Vector2 size = {0.0f, 0.0f}) : Widget(LayoutConfig{.size = size}) {}

        Vector2 measureContent() const override;
        void draw(RenderQueue& rq) const override;
    };

} // namespace rlge::ui
