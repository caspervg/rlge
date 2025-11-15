#pragma once
#include <functional>
#include <vector>

#include "raylib.h"

namespace rlge {
    enum class RenderLayer {
        Background = 0,
        World = 1,
        Foreground = 2,
        UI = 3
    };

    struct DrawCommand {
        RenderLayer layer;
        float z;
        std::function<void()> draw;
    };

    class RenderQueue {
    public:
        void submit(RenderLayer layer, float z, std::function<void()> fn);
        void submit(RenderLayer layer, std::function<void()> fn);
        void clear();
        void submitBackground(std::function<void()> fn);
        void submitBackground(float z, std::function<void()> fn);
        void submitWorld(std::function<void()> fn);
        void submitWorld(float z, std::function<void()> fn);
        void submitForeground(std::function<void()> fn);
        void submitForeground(float z, std::function<void()> fn);
        void submitUI(std::function<void()> fn);
        void flush(const Camera2D& cam);

    private:
        std::vector<DrawCommand> commands_;
    };
}
