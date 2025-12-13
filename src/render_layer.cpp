#include "render_layer.hpp"

#include <algorithm>

namespace rlge {

    LayerRegistry::LayerRegistry() = default;

    LayerId LayerRegistry::create(std::string_view name, int sortOrder, bool worldSpace) {
        LayerId id = nextId_++;

        LayerData data;
        data.id = id;
        data.config.name = std::string(name);
        data.config.sortOrder = sortOrder;
        data.config.worldSpace = worldSpace;

        layers_.emplace(id, std::move(data));
        return id;
    }

    bool LayerRegistry::remove(LayerId id) {
        return layers_.erase(id) > 0;
    }

    std::optional<std::reference_wrapper<LayerData>> LayerRegistry::get(LayerId id) {
        auto it = layers_.find(id);
        if (it != layers_.end()) {
            return std::ref(it->second);
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const LayerData>> LayerRegistry::get(LayerId id) const {
        auto it = layers_.find(id);
        if (it != layers_.end()) {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<LayerData>> LayerRegistry::getByName(std::string_view name) {
        for (auto& [id, layer] : layers_) {
            if (layer.config.name == name) {
                return std::ref(layer);
            }
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const LayerData>> LayerRegistry::getByName(std::string_view name) const {
        for (const auto& [id, layer] : layers_) {
            if (layer.config.name == name) {
                return std::cref(layer);
            }
        }
        return std::nullopt;
    }

    void LayerRegistry::setShader(LayerId id, Shader shader) {
        if (auto layer = get(id)) {
            layer->get().shader = shader;
            layer->get().shaderHandle = InvalidShaderHandle;
            layer->get().shaderParams.reset();
        }
    }

    void LayerRegistry::setShader(LayerId id, ShaderHandle handle, Shader shader) {
        if (auto layer = get(id)) {
            layer->get().shader = shader;
            layer->get().shaderHandle = handle;
            layer->get().shaderParams.reset();
        }
    }

    void LayerRegistry::clearShader(LayerId id) {
        if (auto layer = get(id)) {
            layer->get().shader = {0};
            layer->get().shaderParams.reset();
        }
    }

    template<typename Self>
    auto LayerRegistry::getSortedImpl(Self& self) {
        using LayerPtr = std::conditional_t<std::is_const_v<Self>, const LayerData*, LayerData*>;
        std::vector<LayerPtr> result;
        result.reserve(self.layers_.size());
        for (auto& [id, layer] : self.layers_) {
            result.push_back(&layer);
        }
        std::sort(result.begin(), result.end(),
                  [](const auto* a, const auto* b) {
                      return a->config.sortOrder < b->config.sortOrder;
                  });
        return result;
    }

    std::vector<LayerData*> LayerRegistry::getSorted() {
        return getSortedImpl(*this);
    }

    std::vector<const LayerData*> LayerRegistry::getSorted() const {
        return getSortedImpl(*this);
    }

    std::vector<std::reference_wrapper<LayerData>> LayerRegistry::all() {
        std::vector<std::reference_wrapper<LayerData>> result;
        result.reserve(layers_.size());
        for (auto& [_, layer] : layers_) {
            result.push_back(std::ref(layer));
        }
        return result;
    }

    std::vector<std::reference_wrapper<const LayerData>> LayerRegistry::all() const {
        std::vector<std::reference_wrapper<const LayerData>> result;
        result.reserve(layers_.size());
        for (const auto& [_, layer] : layers_) {
            result.push_back(std::cref(layer));
        }
        return result;
    }

    void LayerRegistry::refreshShader(const ShaderHandle handle, const Shader newShader) {
        for (auto& [_, layer] : layers_) {
            if (layer.shaderHandle == handle) {
                layer.shader = newShader;
                if (layer.shaderParams) {
                    layer.shaderParams->setShader(newShader);
                }
            }
        }
    }

    void LayerRegistry::createDefaults() {
        // Create default layers matching old RenderLayer enum behavior
        backgroundId_ = create("background", 0, true);
        worldId_ = create("world", 50, true);
        foregroundId_ = create("foreground", 100, true);
        uiId_ = create("ui", 1000, false);  // Screen-space
    }

} // namespace rlge
