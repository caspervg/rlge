#include "prefab.hpp"

namespace rlge {
    void PrefabFactory::registerPrefab(const std::string& name, Fn fn) {
        registry_[name] = std::move(fn);
    }

    Entity* PrefabFactory::instantiate(const std::string& name, Scene& scene) const {
        auto it = registry_.find(name);
        if (it == registry_.end())
            return nullptr;
        return &it->second(scene);
    }
}

