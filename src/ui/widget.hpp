#pragma once
#include <memory>
#include <string>
#include <vector>

#include "raylib.h"

#include "render_queue.hpp"
#include "layout.hpp"

namespace rlge::ui {

    // Base widget node with layout and children support.
    class Widget {
    public:
        explicit Widget(LayoutConfig layout = {})
            : layout_(layout) {}
        virtual ~Widget() = default;

        // Measure desired content size (excluding padding) for this widget.
        virtual Vector2 measureContent() const = 0;

        // Compute final bounds given parent space; derived classes can override.
        virtual void computeLayout(const Rectangle& parentBounds);

        // Draw this widget (and children by default).
        virtual void draw(RenderQueue& rq) const;

        // Accessors
        [[nodiscard]] const LayoutConfig& layout() const { return layout_; }
        void setLayout(const LayoutConfig& l) { layout_ = l; invalidateLayout(); }

        [[nodiscard]] Rectangle bounds() const { return computedBounds_; }
        [[nodiscard]] Rectangle contentBounds() const;

        [[nodiscard]] bool visible() const { return visible_; }
        void setVisible(bool v) { visible_ = v; invalidateLayout(); }

        // Identifier for state lookup (optional but recommended)
        void setId(std::string id) { id_ = std::move(id); }
        [[nodiscard]] const std::string& id() const { return id_; }

        void invalidateLayout() { layoutDirty_ = true; }

        template <typename T, typename... Args>
        T& addChild(Args&&... args) {
            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            auto& ref = *child;
            children_.push_back(std::move(child));
            invalidateLayout();
            return ref;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }
        void clearChildren() { children_.clear(); invalidateLayout(); }

    protected:
        LayoutConfig layout_{};
        std::vector<std::unique_ptr<Widget>> children_;
        Rectangle computedBounds_{0.0f, 0.0f, 0.0f, 0.0f};
        bool layoutDirty_{true};
        bool visible_{true};
        std::string id_{};
    };

} // namespace rlge::ui
