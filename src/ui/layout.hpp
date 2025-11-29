#pragma once
#include "raylib.h"

namespace rlge::ui {

    enum class Alignment {
        Start,
        Center,
        End,
        Stretch
    };

    enum class Distribution {
        Start,
        Center,
        End,
        SpaceBetween,
        SpaceAround,
        SpaceEvenly
    };

    struct LayoutConfig {
        // Desired size; negative means use measured size or available space.
        Vector2 size{-1.0f, -1.0f};
        // Padding applied inside the widget bounds.
        Vector2 padding{0.0f, 0.0f};
    };

} // namespace rlge::ui
