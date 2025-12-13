#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "shader_params.hpp"
#include "asset.hpp"

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
        ShaderHandle shaderHandle{InvalidShaderHandle};
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
        LayerId create(std::string_view name, int sortOrder, bool worldSpace = true);

        // Remove a layer by ID
        // Returns true if the layer was found and removed
        bool remove(LayerId id);

        // Get layer data by ID
        std::optional<std::reference_wrapper<LayerData>> get(LayerId id);
        std::optional<std::reference_wrapper<const LayerData>> get(LayerId id) const;

        // Get layer by name
        std::optional<std::reference_wrapper<LayerData>> getByName(std::string_view name);
        std::optional<std::reference_wrapper<const LayerData>> getByName(std::string_view name) const;

        // Set a simple shader for a layer (no typed params)
        void setShader(LayerId id, Shader shader);
        void setShader(LayerId id, ShaderHandle handle, Shader shader);

        // Set typed shader params for a layer
        template<typename T>
        void setShaderParams(LayerId id, ShaderParams<T> params) {
            if (auto layer = get(id)) {
                layer->get().shader = params.shader();
                layer->get().shaderHandle = InvalidShaderHandle;
                layer->get().shaderParams = std::make_unique<ShaderParamsWrapper<T>>(
                    std::move(params));
            }
        }

        template<typename T>
        void setShaderParams(LayerId id, ShaderHandle handle, ShaderParams<T> params) {
            if (auto layer = get(id)) {
                layer->get().shader = params.shader();
                layer->get().shaderHandle = handle;
                layer->get().shaderParams = std::make_unique<ShaderParamsWrapper<T>>(
                    std::move(params));
            }
        }

        // Clear shader from a layer
        void clearShader(LayerId id);

        // Get all layers sorted by sort order
        std::vector<LayerData*> getSorted();
        std::vector<const LayerData*> getSorted() const;
        std::vector<std::reference_wrapper<LayerData>> all();
        std::vector<std::reference_wrapper<const LayerData>> all() const;

        void refreshShader(ShaderHandle handle, Shader newShader);

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

        // Helper template to avoid code duplication in getSorted
        template<typename Self>
        static auto getSortedImpl(Self& self);
    };

} // namespace rlge
