#pragma once
#include <algorithm>
#include <functional>
#include <utility>
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
        void submit(RenderLayer layer, float z, std::function<void()> fn) {
            commands_.push_back(DrawCommand{layer, z, std::move(fn)});
        }

        // Overload with optional z (defaults to 0.0f)
        void submit(RenderLayer layer, std::function<void()> fn) {
            submit(layer, 0.0f, std::move(fn));
        }

        void clear() {
            commands_.clear();
        }

        // Convenience helpers for common cases
        void submitBackground(std::function<void()> fn) {
            submit(RenderLayer::Background, std::move(fn));
        }

        void submitBackground(float z, std::function<void()> fn) {
            submit(RenderLayer::Background, z, std::move(fn));
        }

        void submitWorld(std::function<void()> fn) {
            submit(RenderLayer::World, std::move(fn));
        }

        void submitWorld(float z, std::function<void()> fn) {
            submit(RenderLayer::World, z, std::move(fn));
        }

        void submitForeground(std::function<void()> fn) {
            submit(RenderLayer::Foreground, std::move(fn));
        }

        void submitForeground(float z, std::function<void()> fn) {
            submit(RenderLayer::Foreground, z, std::move(fn));
        }

        void submitUI(std::function<void()> fn) {
            submit(RenderLayer::UI, std::move(fn));
        }

        void flush(const Camera2D& cam) {
            if (commands_.empty())
                return;

            std::sort(commands_.begin(), commands_.end(),
                      [](const DrawCommand& a, const DrawCommand& b) {
                          if (a.layer != b.layer)
                              return static_cast<int>(a.layer) < static_cast<int>(b.layer);
                          return a.z < b.z;
                      });

            // World-space layers (with camera)
            BeginMode2D(cam);
            for (const auto& cmd : commands_) {
                if (cmd.layer == RenderLayer::UI)
                    continue;
                if (cmd.draw)
                    cmd.draw();
            }
            EndMode2D();

            // UI layer (screen-space)
            for (const auto& cmd : commands_) {
                if (cmd.layer != RenderLayer::UI)
                    continue;
                if (cmd.draw)
                    cmd.draw();
            }

            commands_.clear();
        }

    private:
        std::vector<DrawCommand> commands_;
    };
}
