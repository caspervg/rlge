#pragma once
#include <functional>
#include <vector>
#include <unordered_map>

#include "raylib.h"
#include "render_layer.hpp"

namespace rlge {
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
        LayerId layer = InvalidLayerId;
        Texture2D texture;
        std::vector<SpriteQuad> quads;

        void clear() { quads.clear(); }
        void reserve(size_t n) { quads.reserve(n); }
    };

    // Custom draw command (can use Shader for per-entity effects)
    struct DrawCommand {
        LayerId layer = InvalidLayerId;
        float z;
        std::function<void()> draw;
        Shader shader = {0};  // Optional custom shader
    };

    // Performance metrics
    struct RenderStats {
        size_t spritesSubmitted = 0;
        size_t batchCount = 0;
        size_t drawCalls = 0;
        size_t customCommands = 0;
        size_t viewsRendered = 0;
        size_t executedDrawCalls = 0;
        float sortTimeMs = 0.0f;
        float flushTimeMs = 0.0f;

        void reset() {
            spritesSubmitted = 0;
            batchCount = 0;
            drawCalls = 0;
            customCommands = 0;
            viewsRendered = 0;
            executedDrawCalls = 0;
            sortTimeMs = 0.0f;
            flushTimeMs = 0.0f;
        }
    };

    class RenderQueue {
    public:
        // Constructor with injected LayerRegistry (required for dependency injection)
        explicit RenderQueue(LayerRegistry& layers);

        // Get the layer registry
        LayerRegistry& layers() { return layers_; }
        const LayerRegistry& layers() const { return layers_; }

        // LayerId-based sprite submission
        void submitSprite(LayerId layer, float z, Texture2D texture,
                         Rectangle src, Rectangle dest, Vector2 origin,
                         float rotation, Color tint = WHITE);

        // Custom draw with optional shader (for per-entity effects)
        void submitCustom(LayerId layer, float z, Shader shader,
                         std::function<void()> fn);

        // LayerId-based lambda submission
        void submit(LayerId layer, float z, std::function<void()> fn);
        void submit(LayerId layer, std::function<void()> fn);

        // Convenience methods (use default layer IDs from registry)
        void submitBackground(std::function<void()> fn);
        void submitBackground(float z, std::function<void()> fn);
        void submitWorld(std::function<void()> fn);
        void submitWorld(float z, std::function<void()> fn);
        void submitForeground(std::function<void()> fn);
        void submitForeground(float z, std::function<void()> fn);
        void submitUI(std::function<void()> fn);

        void beginFrame();
        void clear();
        // Prepare world-space data (sorting batches/commands) once per frame.
        void prepareWorld();
        // Render prepared world-space layers for a given camera and viewport.
        void flushPreparedWorld(const Camera2D& cam, const Rectangle& viewport);
        // Render UI layer (screen-space). Clears the queue.
        void flushUI();

        // Get performance stats
        const RenderStats& stats() const { return stats_; }

    private:
        LayerRegistry& layers_;

        // Batch management per layer
        using TextureId = unsigned int;  // texture.id from raylib
        std::unordered_map<LayerId, std::unordered_map<TextureId, SpriteBatch>> batches_;

        // Custom draw commands
        std::vector<DrawCommand> commands_;

        // Stats
        RenderStats stats_;
        bool worldPrepared_ = false;

        // Helper methods
        SpriteBatch& getBatch(LayerId layer, Texture2D texture);

        // Comparator for sorting draw commands by layer order then z
        static bool compareDrawCommands(const LayerRegistry& layers,
                                        const DrawCommand& a,
                                        const DrawCommand& b);

        // Comparator for sorting sprite quads by z
        static bool compareQuadsByZ(const SpriteQuad& a, const SpriteQuad& b);
    };
}
