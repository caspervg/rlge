#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "camera.hpp"
#include "raylib.h"

namespace rlge {
    using ViewId = std::uint32_t;

    enum class ResizeMode {
        Fill,
        Letterbox
    };

    struct View {
        ViewId id;
        std::reference_wrapper<Camera> camera;
        Rectangle viewport;
        std::function<Rectangle(float width, float height)> onResize{nullptr};
        std::optional<ResizeMode> resizeMode{std::nullopt};
        std::optional<float> aspectRatio{std::nullopt};
    };
} // namespace rlge

