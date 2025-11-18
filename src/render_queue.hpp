#pragma once
#include <functional>
#include <vector>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    enum class RenderLayer {
        Background = 0,
        World = 1,
        Foreground = 2,
        UI = 3
    };

    // Batched sprite quad data
    struct SpriteQuad {
        Rectangle src;
        Rectangle dest;
        Vector2 origin;
        float rotation;
        Color tint;
        float z;  // For sorting within batch
    };

    // Batch of sprites sharing the same texture
    struct SpriteBatch {
        RenderLayer layer;
        Texture2D texture;
        std::vector<SpriteQuad> quads;

        void clear() { quads.clear(); }
        void reserve(size_t n) { quads.reserve(n); }
    };

    // Legacy draw command for custom drawing
    struct DrawCommand {
        RenderLayer layer;
        float z;
        std::function<void()> draw;
    };

    // Performance metrics
    struct RenderStats {
        size_t spritesSubmitted = 0;
        size_t batchCount = 0;
        size_t drawCalls = 0;
        size_t customCommands = 0;
        float sortTimeMs = 0.0f;
        float flushTimeMs = 0.0f;

        void reset() {
            spritesSubmitted = 0;
            batchCount = 0;
            drawCalls = 0;
            customCommands = 0;
            sortTimeMs = 0.0f;
            flushTimeMs = 0.0f;
        }
    };

    class RenderQueue {
    public:
        RenderQueue();

        // Batched sprite submission (preferred)
        void submitSprite(RenderLayer layer, float z, Texture2D texture,
                         Rectangle src, Rectangle dest, Vector2 origin,
                         float rotation, Color tint = WHITE);

        // Legacy lambda-based submission (for custom drawing)
        void submit(RenderLayer layer, float z, std::function<void()> fn);
        void submit(RenderLayer layer, std::function<void()> fn);

        // Convenience methods
        void submitBackground(std::function<void()> fn);
        void submitBackground(float z, std::function<void()> fn);
        void submitWorld(std::function<void()> fn);
        void submitWorld(float z, std::function<void()> fn);
        void submitForeground(std::function<void()> fn);
        void submitForeground(float z, std::function<void()> fn);
        void submitUI(std::function<void()> fn);

        void clear();
        // Render world-space layers (Background, World, Foreground) with a given camera and viewport.
        // Does not clear the queue so it can be called multiple times (for multiple cameras/views).
        void flushWorld(const Camera2D& cam, const Rectangle& viewport);
        // Render UI layer (screen-space). Clears the queue and stats.
        void flushUI();
        // Legacy single-camera flush: world + UI in one call.
        void flush(const Camera2D& cam);

        // Get performance stats
        const RenderStats& stats() const { return stats_; }

    private:
        // Batch management
        using TextureId = unsigned int;  // texture.id from raylib
        std::unordered_map<TextureId, SpriteBatch> batches_[4];  // One per layer

        // Custom draw commands
        std::vector<DrawCommand> commands_;

        // Stats
        RenderStats stats_;

        // Helper methods
        SpriteBatch& getBatch(RenderLayer layer, Texture2D texture);
    };
}
