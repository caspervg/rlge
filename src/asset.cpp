#include "asset.hpp"

namespace rlge {
    AssetStore::~AssetStore() {
        unloadAll();
    }

    Texture2D& AssetStore::loadTexture(const std::string& id, const std::string& path) {
        const auto it = textures_.find(id);
        if (it != textures_.end())
            return it->second;
        Texture2D tex = LoadTexture(path.c_str());
        auto [iter, _] = textures_.emplace(id, tex);
        return iter->second;
    }

    Texture2D& AssetStore::texture(const std::string& id) {
        return textures_.at(id);
    }

    void AssetStore::unloadAll() {
        for (const auto& kv : textures_) {
            UnloadTexture(kv.second);
        }
        textures_.clear();
    }
}

