#pragma once
#include <ranges>
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    class AssetStore {
    public:
        AssetStore() = default;
        ~AssetStore() { unloadAll(); }

        AssetStore(const AssetStore&) = delete;
        AssetStore& operator=(const AssetStore&) = delete;

        Texture2D& loadTexture(const std::string& id, const std::string& path) {
            auto it = textures_.find(id);
            if (it != textures_.end())
                return it->second;
            Texture2D tex = LoadTexture(path.c_str());
            auto [iter, _] = textures_.emplace(id, tex);
            return iter->second;
        }

        Texture2D& texture(const std::string& id) {
            return textures_.at(id);
        }

        void unloadAll() {
            for (const auto& val : textures_ | std::views::values) {
                UnloadTexture(val);
            }
            textures_.clear();
        }

    private:
        std::unordered_map<std::string, Texture2D> textures_;
    };
}
