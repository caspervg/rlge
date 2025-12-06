#pragma once

#include <optional>

#include "input.hpp"
#include "raylib.h"
#include "view.hpp"

namespace rlge {

    struct WindowConfig {
        float width{1280.0f};
        float height{720.0f};
        int fps{60};
        const char* title{"RLGE Game"};

        unsigned int flags{FLAG_VSYNC_HINT};

        bool resizable{false};
        bool startFullscreen{false};
        bool borderless{false};

        ResizeMode resizeMode{ResizeMode::Fill};
        float aspectRatio{0.0f}; // 0 -> derive from width/height
        std::optional<KeyCode> fullscreenKey{std::nullopt};

    #if NDEBUG
        std::optional<KeyCode> debugKey{KeyCode::F12};
    #else
        std::optional<KeyCode> debugKey{std::nullopt};
    #endif
    };

    class Window {
    public:
        explicit Window(const WindowConfig& cfg);
        ~Window();

        void toggleFullscreen();
        void toggleBorderlessWindowed();

        void setSize(int width, int height);
        void setPosition(int x, int y);
        void setTitle(const char* title);

        void setIcon(Image image);

        [[nodiscard]] Vector2 size() const;
        [[nodiscard]] Vector2 renderSize() const;
        [[nodiscard]] bool isFullscreen() const;
        [[nodiscard]] bool isFocused() const;
        [[nodiscard]] Vector2 dpiScale() const;

        [[nodiscard]] void* nativeHandle() const;
    };

} // namespace rlge
