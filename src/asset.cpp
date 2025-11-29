#include "asset.hpp"
#include <utility>

namespace rlge {
    AssetStore::~AssetStore() {
        unloadAll();
    }

    void AssetStore::setRoot(std::filesystem::path root) {
        // Normalize and prefer absolute so joins are predictable.
        if (!root.is_absolute()) {
            root = std::filesystem::absolute(root);
        }
        assetRoot_ = root.lexically_normal();
    }

    std::filesystem::path AssetStore::path(std::string_view relative) const {
        std::filesystem::path relPath(relative);
        if (relPath.is_absolute()) {
            return relPath.lexically_normal();
        }
        return (assetRoot_ / relPath).lexically_normal();
    }

    Texture2D& AssetStore::loadTexture(const std::string& id, const std::string& path) {
        const auto it = textures_.find(id);
        if (it != textures_.end())
            return it->second;
        const auto fullPath = this->path(path);
        Texture2D tex = LoadTexture(fullPath.string().c_str());
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
