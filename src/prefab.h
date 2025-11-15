#pragma once
#include <functional>
#include <string>
#include <unordered_map>

namespace rlge {
    class Scene;
    class Entity;

    class PrefabFactory {
    public:
        using Fn = std::function<Entity&(Scene&)>;

        void registerPrefab(const std::string& name, Fn fn);
        Entity* instantiate(const std::string& name, Scene& scene) const;

    private:
        std::unordered_map<std::string, Fn> registry_;
    };

}
