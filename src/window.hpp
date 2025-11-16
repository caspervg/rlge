#pragma once

#include "raylib.h"

namespace rlge {

    struct WindowConfig {
        int width{1280};
        int height{720};
        int fps{60};
        const char* title{"RLGE Game"};

        unsigned int flags{FLAG_VSYNC_HINT};

        bool resizable{false};
        bool startFullscreen{false};
        bool borderless{false};
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
        [[nodiscard]] bool isFullscreen() const;
        [[nodiscard]] bool isFocused() const;
        [[nodiscard]] Vector2 dpiScale() const;

        [[nodiscard]] void* nativeHandle() const;
    };

} // namespace rlge
