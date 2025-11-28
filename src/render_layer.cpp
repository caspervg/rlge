#include "render_layer.hpp"

#include <algorithm>

namespace rlge {

    LayerRegistry::LayerRegistry() = default;

    LayerId LayerRegistry::create(const std::string& name, int sortOrder, bool worldSpace) {
        LayerId id = nextId_++;

        LayerData data;
        data.id = id;
        data.config.name = name;
        data.config.sortOrder = sortOrder;
        data.config.worldSpace = worldSpace;

        layers_.emplace(id, std::move(data));
        return id;
    }

    bool LayerRegistry::remove(LayerId id) {
        return layers_.erase(id) > 0;
    }

    LayerData* LayerRegistry::get(LayerId id) {
        auto it = layers_.find(id);
        return it != layers_.end() ? &it->second : nullptr;
    }

    const LayerData* LayerRegistry::get(LayerId id) const {
        auto it = layers_.find(id);
        return it != layers_.end() ? &it->second : nullptr;
    }

    LayerData* LayerRegistry::getByName(const std::string& name) {
        for (auto& [id, layer] : layers_) {
            if (layer.config.name == name) {
                return &layer;
            }
        }
        return nullptr;
    }

    const LayerData* LayerRegistry::getByName(const std::string& name) const {
        for (const auto& [id, layer] : layers_) {
            if (layer.config.name == name) {
                return &layer;
            }
        }
        return nullptr;
    }

    void LayerRegistry::setShader(LayerId id, Shader shader) {
        auto* layer = get(id);
        if (layer) {
            layer->shader = shader;
            layer->shaderParams.reset();
        }
    }

    void LayerRegistry::clearShader(LayerId id) {
        auto* layer = get(id);
        if (layer) {
            layer->shader = {0};
            layer->shaderParams.reset();
        }
    }

    std::vector<LayerData*> LayerRegistry::getSorted() {
        std::vector<LayerData*> result;
        result.reserve(layers_.size());
        for (auto& [id, layer] : layers_) {
            result.push_back(&layer);
        }
        std::sort(result.begin(), result.end(),
                  [](const LayerData* a, const LayerData* b) {
                      return a->config.sortOrder < b->config.sortOrder;
                  });
        return result;
    }

    std::vector<const LayerData*> LayerRegistry::getSorted() const {
        std::vector<const LayerData*> result;
        result.reserve(layers_.size());
        for (const auto& [id, layer] : layers_) {
            result.push_back(&layer);
        }
        std::sort(result.begin(), result.end(),
                  [](const LayerData* a, const LayerData* b) {
                      return a->config.sortOrder < b->config.sortOrder;
                  });
        return result;
    }

    void LayerRegistry::createDefaults() {
        // Create default layers matching old RenderLayer enum behavior
        backgroundId_ = create("background", 0, true);
        worldId_ = create("world", 50, true);
        foregroundId_ = create("foreground", 100, true);
        uiId_ = create("ui", 1000, false);  // Screen-space
    }

} // namespace rlge
