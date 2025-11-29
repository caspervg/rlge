#include "spacer.hpp"

namespace rlge::ui {

    Vector2 Spacer::measureContent() const {
        auto size = layout_.size;
        if (size.x < 0.0f) size.x = 0.0f;
        if (size.y < 0.0f) size.y = 0.0f;
        size.x += layout_.padding.x * 2.0f;
        size.y += layout_.padding.y * 2.0f;
        return size;
    }

    void Spacer::draw(RenderQueue& rq) const {
        (void)rq; // intentionally empty
    }

} // namespace rlge::ui
