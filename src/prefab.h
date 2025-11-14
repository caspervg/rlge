#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace rlge {
    class Scene;
    class Entity;

    class PrefabFactory {
    public:
        using Fn = std::function<Entity&(Scene&)>;

        void registerPrefab(const std::string& name, Fn fn) {
            registry_[name] = std::move(fn);
        }

        Entity* instantiate(const std::string& name, Scene& scene) const {
            auto it = registry_.find(name);
            if (it == registry_.end())
                return nullptr;
            return &it->second(scene);
        }

    private:
        std::unordered_map<std::string, Fn> registry_;
    };

}
