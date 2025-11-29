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

    Shader& AssetStore::loadShader(const std::string& id, const std::string& vertPath, const std::string& fragPath) {
        const auto it = shaders_.find(id);
        if (it != shaders_.end())
            return it->second;
        const auto v = this->path(vertPath);
        const auto f = this->path(fragPath);
        Shader shader = LoadShader(v.string().c_str(), f.string().c_str());
        auto [iter, _] = shaders_.emplace(id, shader);
        return iter->second;
    }

    Shader& AssetStore::loadShaderFromMemory(const std::string& id, const char* vertSrc, const char* fragSrc) {
        const auto it = shaders_.find(id);
        if (it != shaders_.end())
            return it->second;
        Shader shader = LoadShaderFromMemory(vertSrc, fragSrc);
        auto [iter, _] = shaders_.emplace(id, shader);
        return iter->second;
    }

    Shader& AssetStore::loadVertexShader(const std::string& id, const std::string& vertPath) {
        const auto it = shaders_.find(id);
        if (it != shaders_.end())
            return it->second;
        const auto v = this->path(vertPath);
        Shader shader = LoadShader(v.string().c_str(), nullptr);
        auto [iter, _] = shaders_.emplace(id, shader);
        return iter->second;
    }

    Shader& AssetStore::loadFragmentShader(const std::string& id, const std::string& fragPath) {
        const auto it = shaders_.find(id);
        if (it != shaders_.end())
            return it->second;
        const auto f = this->path(fragPath);
        Shader shader = LoadShader(nullptr, f.string().c_str());
        auto [iter, _] = shaders_.emplace(id, shader);
        return iter->second;
    }

    Shader& AssetStore::shader(const std::string& id) {
        return shaders_.at(id);
    }

    void AssetStore::unloadAll() {
        for (const auto& kv : shaders_) {
            UnloadShader(kv.second);
        }
        shaders_.clear();
        for (const auto& kv : textures_) {
            UnloadTexture(kv.second);
        }
        textures_.clear();
    }
}
