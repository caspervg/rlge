#include "render_queue.hpp"

#include <algorithm>

namespace rlge {
    void RenderQueue::submit(RenderLayer layer, float z, std::function<void()> fn) {
        commands_.push_back(DrawCommand{layer, z, std::move(fn)});
    }

    void RenderQueue::submit(RenderLayer layer, std::function<void()> fn) {
        submit(layer, 0.0f, std::move(fn));
    }

    void RenderQueue::clear() {
        commands_.clear();
    }

    void RenderQueue::submitBackground(std::function<void()> fn) {
        submit(RenderLayer::Background, std::move(fn));
    }

    void RenderQueue::submitBackground(float z, std::function<void()> fn) {
        submit(RenderLayer::Background, z, std::move(fn));
    }

    void RenderQueue::submitWorld(std::function<void()> fn) {
        submit(RenderLayer::World, std::move(fn));
    }

    void RenderQueue::submitWorld(float z, std::function<void()> fn) {
        submit(RenderLayer::World, z, std::move(fn));
    }

    void RenderQueue::submitForeground(std::function<void()> fn) {
        submit(RenderLayer::Foreground, std::move(fn));
    }

    void RenderQueue::submitForeground(float z, std::function<void()> fn) {
        submit(RenderLayer::Foreground, z, std::move(fn));
    }

    void RenderQueue::submitUI(std::function<void()> fn) {
        submit(RenderLayer::UI, std::move(fn));
    }

    void RenderQueue::flush(const Camera2D& cam) {
        if (commands_.empty())
            return;

        std::sort(commands_.begin(), commands_.end(),
                  [](const DrawCommand& a, const DrawCommand& b) {
                      if (a.layer != b.layer)
                          return static_cast<int>(a.layer) < static_cast<int>(b.layer);
                      return a.z < b.z;
                  });

        BeginMode2D(cam);
        for (const auto& cmd : commands_) {
            if (cmd.layer == RenderLayer::UI)
                continue;
            if (cmd.draw)
                cmd.draw();
        }
        EndMode2D();

        for (const auto& cmd : commands_) {
            if (cmd.layer != RenderLayer::UI)
                continue;
            if (cmd.draw)
                cmd.draw();
        }

        commands_.clear();
    }
}

