#include "window.hpp"

namespace rlge {

    Window::Window(const WindowConfig& cfg) {
        unsigned int cfgFlags = cfg.flags;
        if (cfg.resizable) {
            cfgFlags |= FLAG_WINDOW_RESIZABLE;
        }
        if (cfg.borderless) {
            cfgFlags |= FLAG_WINDOW_UNDECORATED;
        }
        if (cfg.startFullscreen) {
            cfgFlags |= FLAG_FULLSCREEN_MODE;
        }

        SetConfigFlags(cfgFlags);
        InitWindow(cfg.width, cfg.height, cfg.title);
        SetTargetFPS(cfg.fps);
    }

    Window::~Window() {
        if (IsWindowReady()) {
            CloseWindow();
        }
    }

    void Window::toggleFullscreen() {
        ToggleFullscreen();
    }

    void Window::toggleBorderlessWindowed() {
        ToggleBorderlessWindowed();
    }

    void Window::setSize(const int width, const int height) {
        SetWindowSize(width, height);
    }

    void Window::setPosition(const int x, const int y) {
        SetWindowPosition(x, y);
    }

    void Window::setTitle(const char* title) {
        SetWindowTitle(title);
    }

    void Window::setIcon(const Image image) {
        SetWindowIcon(image);
    }

    Vector2 Window::size() const {
        return Vector2{
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())
        };
    }

    bool Window::isFullscreen() const {
        return IsWindowFullscreen();
    }

    bool Window::isFocused() const {
        return IsWindowFocused();
    }

    Vector2 Window::dpiScale() const {
        return GetWindowScaleDPI();
    }

    void* Window::nativeHandle() const {
        return GetWindowHandle();
    }

} // namespace rlge

