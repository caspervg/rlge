#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "shader_params.hpp"

namespace rlge {

    // Unique layer identifier
    using LayerId = std::uint32_t;

    // Invalid layer ID constant
    constexpr LayerId InvalidLayerId = 0;

    // Configuration for a render layer
    struct LayerConfig {
        std::string name;
        int sortOrder = 0;
        bool worldSpace = true;  // false for screen-space (UI-like) layers
    };

    // Internal layer data with optional shader
    struct LayerData {
        LayerId id = InvalidLayerId;
        LayerConfig config;
        Shader shader = {0};  // Optional layer shader
        std::unique_ptr<IShaderParams> shaderParams;  // Optional typed params
    };

    // Forward declaration
    class RenderQueue;

    // Registry for managing dynamic render layers
    class LayerRegistry {
    public:
        LayerRegistry();
        ~LayerRegistry() = default;

        // Create a new layer with the given configuration
        // Returns the unique layer ID
        LayerId create(const std::string& name, int sortOrder, bool worldSpace = true);

        // Remove a layer by ID
        // Returns true if the layer was found and removed
        bool remove(LayerId id);

        // Get layer data by ID (returns nullptr if not found)
        LayerData* get(LayerId id);
        const LayerData* get(LayerId id) const;

        // Get layer by name (returns nullptr if not found)
        LayerData* getByName(const std::string& name);
        const LayerData* getByName(const std::string& name) const;

        // Set a simple shader for a layer (no typed params)
        void setShader(LayerId id, Shader shader);

        // Set typed shader params for a layer
        template<typename T>
        void setShaderParams(LayerId id, ShaderParams<T> params) {
            auto* layer = get(id);
            if (layer) {
                layer->shader = params.shader();
                layer->shaderParams = std::make_unique<ShaderParamsWrapper<T>>(
                    std::move(params));
            }
        }

        // Clear shader from a layer
        void clearShader(LayerId id);

        // Get all layers sorted by sort order
        std::vector<LayerData*> getSorted();
        std::vector<const LayerData*> getSorted() const;

        // Built-in layer ID accessors (created by createDefaults())
        LayerId background() const { return backgroundId_; }
        LayerId world() const { return worldId_; }
        LayerId foreground() const { return foregroundId_; }
        LayerId ui() const { return uiId_; }

        // Create default layers matching the old RenderLayer enum behavior
        void createDefaults();

    private:
        LayerId nextId_ = 1;  // Start at 1 (0 is InvalidLayerId)
        std::unordered_map<LayerId, LayerData> layers_;

        // Default layer IDs
        LayerId backgroundId_ = InvalidLayerId;
        LayerId worldId_ = InvalidLayerId;
        LayerId foregroundId_ = InvalidLayerId;
        LayerId uiId_ = InvalidLayerId;
    };

} // namespace rlge
