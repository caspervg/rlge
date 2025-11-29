#pragma once
#include "widget.hpp"

namespace rlge::ui {

    struct StackConfig {
        float spacing = 0.0f;                            // Gap between children
        Alignment alignment = Alignment::Start;         // Cross-axis alignment
        Distribution distribution = Distribution::Start; // Main-axis distribution
    };

    // Abstract base for linear layout containers
    class Stack : public Widget {
    public:
        explicit Stack(LayoutConfig layout = {}, StackConfig config = {})
            : Widget(layout)
            , config_(config) {}

        void setSpacing(float spacing) { config_.spacing = spacing; invalidateLayout(); }
        [[nodiscard]] float spacing() const { return config_.spacing; }

        void setAlignment(Alignment align) { config_.alignment = align; invalidateLayout(); }
        [[nodiscard]] Alignment alignment() const { return config_.alignment; }

        void setDistribution(Distribution dist) { config_.distribution = dist; invalidateLayout(); }
        [[nodiscard]] Distribution distribution() const { return config_.distribution; }

        // Builder-style helpers
        Stack& spacing(float s) { setSpacing(s); return *this; }
        Stack& align(Alignment a) { setAlignment(a); return *this; }
        Stack& distribute(Distribution d) { setDistribution(d); return *this; }

    protected:
        Vector2 measureContent() const override;
        void computeLayout(const Rectangle& parentBounds) override;

        // Subclasses define orientation
        virtual bool isVertical() const = 0;

        // Helper to get main/cross axis values
        float mainAxis(Vector2 v) const { return isVertical() ? v.y : v.x; }
        float crossAxis(Vector2 v) const { return isVertical() ? v.x : v.y; }
        Vector2 makeVec(float main, float cross) const {
            return isVertical() ? Vector2{cross, main} : Vector2{main, cross};
        }

        StackConfig config_;
    };

    // Vertical stack - children arranged top to bottom
    class VStack : public Stack {
    public:
        using Stack::Stack;

    protected:
        bool isVertical() const override { return true; }
    };

    // Horizontal stack - children arranged left to right
    class HStack : public Stack {
    public:
        using Stack::Stack;

    protected:
        bool isVertical() const override { return false; }
    };

} // namespace rlge::ui
