#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "camera.hpp"
#include "raylib.h"

namespace rlge {
    using ViewId = std::uint32_t;

    enum class ViewSpace {
        World2D,
        World3D
    };

    enum class ResizeMode {
        Fill,
        Letterbox
    };

    struct View {
        ViewId id;
        std::optional<std::reference_wrapper<Camera2DController>> camera2d;
        std::optional<std::reference_wrapper<Camera3DController>> camera3d;
        Rectangle viewport{};
        std::function<Rectangle(float width, float height)> onResize{nullptr};
        std::optional<ResizeMode> resizeMode{std::nullopt};
        std::optional<float> aspectRatio{std::nullopt};
        ViewSpace space{ViewSpace::World2D};

        Camera2DController* camera2D() {
            return camera2d ? &camera2d->get() : nullptr;
        }

        [[nodiscard]] const Camera2DController* camera2D() const {
            return camera2d ? &camera2d->get() : nullptr;
        }

        Camera3DController* camera3D() {
            return camera3d ? &camera3d->get() : nullptr;
        }

        [[nodiscard]] const Camera3DController* camera3D() const {
            return camera3d ? &camera3d->get() : nullptr;
        }
    };
} // namespace rlge
