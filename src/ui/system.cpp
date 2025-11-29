#include "system.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>

#include "layout.hpp"
#include "raylib.h"

namespace rlge::ui {

    namespace {
        // Depth-first traversal to gather visible widgets in draw order.
        void collectWidgets(const Widget& widget, std::vector<const Widget*>& out) {
            out.push_back(&widget);
            for (const auto& child : widget.children()) {
                if (child->visible()) {
                    collectWidgets(*child, out);
                }
            }
        }
    }

    UiSystem::UiSystem()
        : root_(std::make_unique<Panel>(LayoutConfig{}, PanelStyle{})) {}

    void UiSystem::beginFrame(const Vector2 windowSize) {
        hover_ = nullptr;
        active_ = nullptr;
        clicked_.reset();
        clickedId_.reset();
        if (root_) {
            root_->setLayout(LayoutConfig{.size = windowSize});
        }
    }

    void UiSystem::layout() {
        if (root_) {
            root_->computeLayout(Rectangle{0.0f, 0.0f, root_->layout().size.x, root_->layout().size.y});
        }
    }

    void UiSystem::processInput(const Input<>& input) {
        if (!root_) return;

        // Gather widgets in draw order then hit-test back-to-front.
        std::vector<const Widget*> widgets;
        widgets.reserve(64);
        collectWidgets(*root_, widgets);
        std::unordered_map<std::string, Widget*> idMap;
        idMap.reserve(widgets.size());
        for (auto* w : widgets) {
            if (!w->id().empty()) {
                idMap[w->id()] = const_cast<Widget*>(w);
            }
        }

        // If the previously active widget no longer exists, clear the active ID.
        if (activeId_ && !idMap.contains(*activeId_)) {
            activeId_.reset();
        }

        const auto mousePos = input.mousePosition();
        hover_ = nullptr;
        for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
            const auto* w = *it;
            const auto b = w->bounds();
            if (mousePos.x >= b.x && mousePos.x <= b.x + b.width &&
                mousePos.y >= b.y && mousePos.y <= b.y + b.height) {
                hover_ = const_cast<Widget*>(w);
                break;
            }
        }

        const bool mousePressed = input.mousePressed(MouseButton::Left);
        const bool mouseReleased = input.mouseReleased(MouseButton::Left);

        if (mousePressed && hover_) {
            active_ = hover_;
            activeId_ = hover_->id().empty() ? std::optional<std::string>{} : std::optional<std::string>{hover_->id()};
        }
        // Reconcile active_ from activeId_ if we lost the pointer due to rebuilds.
        if (!active_ && activeId_) {
            if (auto it = idMap.find(*activeId_); it != idMap.end()) {
                active_ = it->second;
            }
        }

        if (mouseReleased) {
            const bool idsMatch = activeId_ && hover_ && !hover_->id().empty() && hover_->id() == *activeId_;
            const bool ptrMatch = active_ && active_ == hover_;
            if (ptrMatch || idsMatch) {
                clicked_ = hover_;
                clickedId_ = hover_ && !hover_->id().empty() ? std::optional<std::string>{hover_->id()} : std::optional<std::string>{};
            }
            active_ = nullptr;
            activeId_.reset();
        }

        // Update button states based on hover/active.
        for (auto* w : widgets) {
            if (auto* btn = dynamic_cast<Button*>(const_cast<Widget*>(w))) {
                if (btn->state() == ButtonState::Disabled)
                    continue;
                if (btn == active_) {
                    btn->setState(ButtonState::Pressed);
                } else if (btn == hover_) {
                    btn->setState(ButtonState::Hovered);
                } else {
                    btn->setState(ButtonState::Normal);
                }
            }
        }

        // Invoke callbacks for clicked buttons.
        if (clicked_) {
            if (auto* btn = dynamic_cast<Button*>(*clicked_)) {
                if (btn->onClick()) {
                    btn->onClick()();
                }
            }
        }
    }

    bool UiSystem::wasClicked(const Widget* w) const {
        return clicked_.has_value() && clicked_.value() == w;
    }

    bool UiSystem::wasClicked(const std::string& id) const {
        return clickedId_.has_value() && clickedId_ == id;
    }

    void UiSystem::render(RenderQueue& rq) {
        if (root_) {
            root_->draw(rq);
        }
    }

} // namespace rlge::ui
