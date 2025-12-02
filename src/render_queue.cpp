#include "render_queue.hpp"

#include <algorithm>
#include <chrono>
#include <ranges>

namespace rlge {
    RenderQueue::RenderQueue(LayerRegistry& layers) :
        layers_(layers) {
        commands_.reserve(256);
        stats_.reset();
        worldPrepared_ = false;
    }

    void RenderQueue::submitSprite(const LayerId layer, const float z, const Texture2D& texture,
                                   const Rectangle& src, const Rectangle& dest, const Vector2 origin,
                                   const float rotation, const Color tint) {
        auto& batch = getBatch(layer, texture);
        batch.quads.push_back(SpriteQuad{src, dest, origin, rotation, tint, z});
        stats_.spritesSubmitted++;
        worldPrepared_ = false;
    }

    void RenderQueue::submitCustom(const LayerId layer, const float z, const Shader shader,
                                   std::function<void()> fn) {
        DrawCommand cmd;
        cmd.layer = layer;
        cmd.z = z;
        cmd.draw = std::move(fn);
        cmd.shader = shader;
        commands_.push_back(std::move(cmd));
        stats_.customCommands++;
        worldPrepared_ = false;
    }

    void RenderQueue::submit(const LayerId layer, const float z, std::function<void()> fn) {
        DrawCommand cmd;
        cmd.layer = layer;
        cmd.z = z;
        cmd.draw = std::move(fn);
        commands_.push_back(std::move(cmd));
        stats_.customCommands++;
        worldPrepared_ = false;
    }

    void RenderQueue::submit(const LayerId layer, std::function<void()> fn) {
        submit(layer, 0.0f, std::move(fn));
    }

    void RenderQueue::submitBackground(std::function<void()> fn) {
        submit(layers_.background(), std::move(fn));
    }

    void RenderQueue::submitBackground(float z, std::function<void()> fn) {
        submit(layers_.background(), z, std::move(fn));
    }

    void RenderQueue::submitWorld(std::function<void()> fn) {
        submit(layers_.world(), std::move(fn));
    }

    void RenderQueue::submitWorld(const float z, std::function<void()> fn) {
        submit(layers_.world(), z, std::move(fn));
    }

    void RenderQueue::submitForeground(std::function<void()> fn) {
        submit(layers_.foreground(), std::move(fn));
    }

    void RenderQueue::submitForeground(const float z, std::function<void()> fn) {
        submit(layers_.foreground(), z, std::move(fn));
    }

    void RenderQueue::submitUI(std::function<void()> fn) {
        submit(layers_.ui(), std::move(fn));
    }

    void RenderQueue::beginFrame() {
        stats_.reset();
        worldPrepared_ = false;
        currentView_.reset();
    }

    void RenderQueue::clear() {
        for (auto& layerBatches : batches_ | std::views::values) {
            for (auto& batch : layerBatches | std::views::values) {
                batch.clear();
            }
        }
        commands_.clear();
        worldPrepared_ = false;
    }

    void RenderQueue::prepareWorld() {
        if (worldPrepared_)
            return;

        const auto startTime = std::chrono::high_resolution_clock::now();

        // Get sorted layers
        const auto sortedLayers = layers_.getSorted();

        // Sort commands by layer sort order, then by z
        if (!commands_.empty()) {
            std::ranges::sort(commands_,
                              [this](const DrawCommand& a, const DrawCommand& b) {
                                  return compareDrawCommands(layers_, a, b);
                              });
        }

        stats_.batchCount = 0;
        stats_.drawCalls = 0;

        // Sort quads within batches for world-space layers
        for (auto* layer : sortedLayers) {
            if (!layer->config.worldSpace)
                continue;

            auto it = batches_.find(layer->id);
            if (it == batches_.end())
                continue;

            for (auto& batch : it->second | std::views::values) {
                if (batch.quads.empty())
                    continue;

                std::ranges::sort(batch.quads, compareQuadsByZ_);
                stats_.batchCount++;
            }
        }

        // Count world-space commands
        const LayerId uiLayerId = layers_.ui();
        const size_t worldCommands = std::ranges::count_if(
            commands_,
            [uiLayerId](const DrawCommand& cmd) { return cmd.layer != uiLayerId; }
        );
        stats_.drawCalls = stats_.batchCount + worldCommands;

        const auto endTime = std::chrono::high_resolution_clock::now();
        stats_.sortTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        worldPrepared_ = true;
    }

    void RenderQueue::flushPreparedWorld(const Camera2D& cam, const Rectangle& viewport) {
        if (!worldPrepared_)
            prepareWorld();

        const auto startTime = std::chrono::high_resolution_clock::now();

        const Vector2 topLeft = GetScreenToWorld2D({viewport.x, viewport.y}, cam);
        const Vector2 bottomRight = GetScreenToWorld2D(
            {viewport.x + viewport.width, viewport.y + viewport.height}, cam);

        const Rectangle viewBounds{
            topLeft.x,
            topLeft.y,
            bottomRight.x - topLeft.x,
            bottomRight.y - topLeft.y
        };

        BeginMode2D(cam);
        currentView_ = RenderViewContext{&cam, viewport};

        size_t drawCallsThisView = 0;

        // Get sorted world-space layers
        auto sortedLayers = layers_.getSorted();

        for (const auto* layer : sortedLayers) {
            // Skip screen-space layers (UI)
            if (!layer->config.worldSpace)
                continue;

            // Apply layer shader if present
            const bool hasLayerShader = layer->shader.id != 0;
            if (hasLayerShader) {
                BeginShaderMode(layer->shader);
                // Apply typed params if present
                if (layer->shaderParams) {
                    layer->shaderParams->apply();
                }
            }

            // Render batched sprites for this layer
            auto batchIt = batches_.find(layer->id);
            if (batchIt != batches_.end()) {
                for (auto& batch : batchIt->second | std::views::values) {
                    if (batch.quads.empty())
                        continue;

                    auto batchRendered = false;
                    for (const auto& quad : batch.quads) {
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
                        batchRendered = true;
                    }

                    if (batchRendered)
                        drawCallsThisView++;
                }
            }

            // End layer shader before processing custom commands
            if (hasLayerShader) {
                EndShaderMode();
            }

            // Render custom draw commands for this layer
            for (const auto& cmd : commands_) {
                if (cmd.layer == layer->id && cmd.draw) {
                    // Apply command-specific shader if present
                    const bool hasCmdShader = cmd.shader.id != 0;
                    if (hasCmdShader) {
                        BeginShaderMode(cmd.shader);
                    }
                    cmd.draw();
                    if (hasCmdShader) {
                        EndShaderMode();
                    }
                    drawCallsThisView++;
                }
            }
        }

        EndMode2D();
        currentView_.reset();

        stats_.viewsRendered++;
        stats_.executedDrawCalls += drawCallsThisView;

        const auto endTime = std::chrono::high_resolution_clock::now();
        stats_.flushTimeMs += std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

    void RenderQueue::flushUI() {
        currentView_.reset();
        const auto startTime = std::chrono::high_resolution_clock::now();

        const LayerId uiLayerId = layers_.ui();
        const auto uiLayerOpt = layers_.get(uiLayerId);

        size_t uiDrawCalls = 0;

        // Apply UI layer shader if present
        const bool hasLayerShader = uiLayerOpt && uiLayerOpt->get().shader.id != 0;
        if (hasLayerShader) {
            BeginShaderMode(uiLayerOpt->get().shader);
            if (uiLayerOpt->get().shaderParams) {
                uiLayerOpt->get().shaderParams->apply();
            }
        }

        // Render UI batches
        auto batchIt = batches_.find(uiLayerId);
        if (batchIt != batches_.end()) {
            for (auto& batch : batchIt->second | std::views::values) {
                if (batch.quads.empty())
                    continue;

                std::ranges::sort(batch.quads, compareQuadsByZ_);

                for (const auto& quad : batch.quads) {
                    DrawTexturePro(batch.texture, quad.src, quad.dest,
                                   quad.origin, quad.rotation, quad.tint);
                }

                stats_.drawCalls++;
                stats_.batchCount++;
                uiDrawCalls++;
            }
        }

        if (hasLayerShader) {
            EndShaderMode();
        }

        // Render UI commands
        for (const auto& cmd : commands_) {
            if (cmd.layer == uiLayerId && cmd.draw) {
                const bool hasCmdShader = cmd.shader.id != 0;
                if (hasCmdShader) {
                    BeginShaderMode(cmd.shader);
                }
                cmd.draw();
                if (hasCmdShader) {
                    EndShaderMode();
                }
                stats_.drawCalls++;
                uiDrawCalls++;
            }
        }

        clear();

        const auto endTime = std::chrono::high_resolution_clock::now();
        stats_.executedDrawCalls += uiDrawCalls;
        stats_.flushTimeMs += std::chrono::duration<float, std::milli>(endTime - startTime).count();
    }

    SpriteBatch& RenderQueue::getBatch(const LayerId layer, const Texture2D& texture) {
        auto& layerBatches = batches_[layer];
        const TextureId texId = texture.id;
        auto it = layerBatches.find(texId);

        if (it == layerBatches.end()) {
            SpriteBatch batch;
            batch.layer = layer;
            batch.texture = texture;
            batch.quads.reserve(64);
            it = layerBatches.emplace(texId, std::move(batch)).first;
        }

        return it->second;
    }

    bool RenderQueue::compareDrawCommands(const LayerRegistry& layers,
                                          const DrawCommand& a,
                                          const DrawCommand& b) {
        auto layerA = layers.get(a.layer);
        auto layerB = layers.get(b.layer);
        const int orderA = layerA ? layerA->get().config.sortOrder : 0;
        const int orderB = layerB ? layerB->get().config.sortOrder : 0;
        if (orderA != orderB)
            return orderA < orderB;
        return a.z < b.z;
    }

    bool RenderQueue::compareQuadsByZ_(const SpriteQuad& a, const SpriteQuad& b) {
        return a.z < b.z;
    }
} // namespace rlge
