#pragma once
#include <memory>
#include <optional>
#include <unordered_map>

#include "render_queue.hpp"
#include "input.hpp"
#include "ui/panel.hpp"
#include "ui/button.hpp"

namespace rlge::ui {

    class UiSystem {
    public:
        UiSystem();

        // Start a new frame; resets root bounds to window size.
        void beginFrame(Vector2 windowSize);

        // Access root widget for building UI.
        [[nodiscard]] Widget& root() { return *root_; }
        [[nodiscard]] const Widget& root() const { return *root_; }

        // Layout and render
        void layout();
        void render(RenderQueue& rq);

        // Hit testing + state updates
        void processInput(const Input<>& input);
        [[nodiscard]] bool wasClicked(const Widget* w) const;
        [[nodiscard]] bool wasClicked(const std::string& id) const;

    private:
        std::unique_ptr<Panel> root_;
        Widget* hover_{nullptr};
        Widget* active_{nullptr};
        std::optional<Widget*> clicked_;
        std::optional<std::string> clickedId_;
        std::optional<std::string> activeId_;
    };

} // namespace rlge::ui
