#include "render_queue.hpp"

#include <algorithm>
#include <chrono>

namespace rlge {
    RenderQueue::RenderQueue() {
        // Pre-allocate space to reduce allocations
        commands_.reserve(256);
        for (auto& layerBatches : batches_) {
            layerBatches.reserve(16);  // Reserve space for common texture count
        }
    }

    void RenderQueue::submitSprite(RenderLayer layer, float z, Texture2D texture,
                                   Rectangle src, Rectangle dest, Vector2 origin,
                                   float rotation, Color tint) {
        auto& batch = getBatch(layer, texture);
        batch.quads.push_back(SpriteQuad{src, dest, origin, rotation, tint, z});
        stats_.spritesSubmitted++;
    }

    SpriteBatch& RenderQueue::getBatch(RenderLayer layer, Texture2D texture) {
        const int layerIdx = static_cast<int>(layer);
        auto& layerBatches = batches_[layerIdx];

        const TextureId texId = texture.id;
        auto it = layerBatches.find(texId);

        if (it == layerBatches.end()) {
            // Create new batch
            SpriteBatch batch;
            batch.layer = layer;
            batch.texture = texture;
            batch.quads.reserve(64);  // Reserve reasonable size
            it = layerBatches.emplace(texId, std::move(batch)).first;
        }

        return it->second;
    }

    void RenderQueue::submit(RenderLayer layer, float z, std::function<void()> fn) {
        commands_.push_back(DrawCommand{layer, z, std::move(fn)});
        stats_.customCommands++;
    }

    void RenderQueue::submit(RenderLayer layer, std::function<void()> fn) {
        submit(layer, 0.0f, std::move(fn));
    }

    void RenderQueue::clear() {
        for (auto& layerBatches : batches_) {
            for (auto& [texId, batch] : layerBatches) {
                batch.clear();
            }
        }
        commands_.clear();
        stats_.reset();
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

    void RenderQueue::flushWorld(const Camera2D& cam, const Rectangle& viewport) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // Compute world-space view bounds for this camera + viewport
        const Vector2 topLeft = GetScreenToWorld2D({viewport.x, viewport.y}, cam);
        const Vector2 bottomRight = GetScreenToWorld2D(
            {viewport.x + viewport.width, viewport.y + viewport.height}, cam);

        const Rectangle viewBounds{
            topLeft.x,
            topLeft.y,
            bottomRight.x - topLeft.x,
            bottomRight.y - topLeft.y
        };

        // Sort custom commands for deterministic draw order
        if (!commands_.empty()) {
            auto sortStart = std::chrono::high_resolution_clock::now();
            std::sort(commands_.begin(), commands_.end(),
                      [](const DrawCommand& a, const DrawCommand& b) {
                          if (a.layer != b.layer)
                              return static_cast<int>(a.layer) < static_cast<int>(b.layer);
                          return a.z < b.z;
                      });
            auto sortEnd = std::chrono::high_resolution_clock::now();
            stats_.sortTimeMs = std::chrono::duration<float, std::milli>(sortEnd - sortStart).count();
        }

        // Flush world-space batches and commands (Background, World, Foreground)
        BeginMode2D(cam);

        for (int i = 0; i <= static_cast<int>(RenderLayer::Foreground); ++i) {
            // Flush batches for this layer
            auto& layerBatches = batches_[i];
            for (auto& [texId, batch] : layerBatches) {
                if (batch.quads.empty()) continue;

                // Sort quads by z within batch
                std::sort(batch.quads.begin(), batch.quads.end(),
                         [](const SpriteQuad& a, const SpriteQuad& b) {
                             return a.z < b.z;
                         });

                // Draw visible quads in this batch
                for (const auto& quad : batch.quads) {
                    // Approximate world-space bounds of the sprite
                    const Rectangle quadBounds{
                        quad.dest.x - quad.origin.x,
                        quad.dest.y - quad.origin.y,
                        std::abs(quad.dest.width),
                        std::abs(quad.dest.height)
                    };

                    if (!CheckCollisionRecs(quadBounds, viewBounds))
                        continue;

                    DrawTexturePro(batch.texture, quad.src, quad.dest,
                                   quad.origin, quad.rotation, quad.tint);
                }
                stats_.drawCalls++;
                stats_.batchCount++;
            }

            // Flush custom commands for this layer
            for (const auto& cmd : commands_) {
                if (static_cast<int>(cmd.layer) == i && cmd.draw) {
                    cmd.draw();
                    stats_.drawCalls++;
                }
            }
        }

        EndMode2D();

        auto endTime = std::chrono::high_resolution_clock::now();
        stats_.flushTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

    void RenderQueue::flushUI() {
        auto startTime = std::chrono::high_resolution_clock::now();

        // UI layer (without camera)
        auto& uiBatches = batches_[static_cast<int>(RenderLayer::UI)];
        for (auto& [texId, batch] : uiBatches) {
            if (batch.quads.empty()) continue;

            std::sort(batch.quads.begin(), batch.quads.end(),
                     [](const SpriteQuad& a, const SpriteQuad& b) {
                         return a.z < b.z;
                     });

            for (const auto& quad : batch.quads) {
                DrawTexturePro(batch.texture, quad.src, quad.dest,
                             quad.origin, quad.rotation, quad.tint);
            }
            stats_.drawCalls++;
            stats_.batchCount++;
        }

        for (const auto& cmd : commands_) {
            if (cmd.layer == RenderLayer::UI && cmd.draw) {
                cmd.draw();
                stats_.drawCalls++;
            }
        }

        clear();

        auto endTime = std::chrono::high_resolution_clock::now();
        stats_.flushTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

    void RenderQueue::flush(const Camera2D& cam) {
        const Rectangle fullViewport{
            0.0f,
            0.0f,
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())
        };
        flushWorld(cam, fullViewport);
        flushUI();
    }
}
