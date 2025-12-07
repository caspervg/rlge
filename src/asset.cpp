#include "asset.hpp"

#include <fstream>
#include <ranges>
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

    std::filesystem::path AssetStore::path(const std::string_view relative) const {
        const std::filesystem::path relPath(relative);
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

    Font& AssetStore::loadFont(const std::string& id, const std::string& path, const int size) {
        const auto it = fonts_.find(id);
        if (it != fonts_.end())
            return it->second;
        Font font = LoadFontEx(this->path(path).string().c_str(), size, nullptr, 0);
        auto [iter, _] = fonts_.emplace(id, font);
        return iter->second;
    }

    Font& AssetStore::font(const std::string& id) {
        return fonts_.at(id);
    }

    Shader& AssetStore::loadShader(const std::string& id, const std::string& vertPath, const std::string& fragPath) {
        const auto it = shaderAssets_.find(id);
        if (it != shaderAssets_.end())
            return it->second.shader;

        const auto v = path(vertPath);
        const auto f = path(fragPath);

        ShaderAsset asset;
        asset.vertPath = v;
        asset.fragPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(v, ec)) {
                asset.lastVertModTime = std::filesystem::last_write_time(v, ec);
            }
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastFragModTime = std::filesystem::last_write_time(f, ec);
            }

            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            reloadShaderAsset_(iter->second);
            return iter->second.shader;
        } else {
            asset.shader = LoadShader(v.string().c_str(), f.string().c_str());
            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            return iter->second.shader;
        }
    }

    Shader& AssetStore::loadVertexShader(const std::string& id, const std::string& vertPath) {
        const auto it = shaderAssets_.find(id);
        if (it != shaderAssets_.end())
            return it->second.shader;

        const auto f = path(vertPath);

        ShaderAsset asset;
        asset.vertPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastVertModTime = std::filesystem::last_write_time(f, ec);
            }
            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            reloadShaderAsset_(iter->second);
            return iter->second.shader;
        } else {
            asset.shader = LoadShader(nullptr, f.string().c_str());
            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            return iter->second.shader;
        }
    }

    Shader& AssetStore::loadFragmentShader(const std::string& id, const std::string& fragPath) {
        const auto it = shaderAssets_.find(id);
        if (it != shaderAssets_.end())
            return it->second.shader;

        const auto f = path(fragPath);

        ShaderAsset asset;
        asset.fragPath = f;
        asset.hotReload = hotReload_;

        if (hotReload_) {
            if (std::error_code ec; std::filesystem::exists(f, ec)) {
                asset.lastFragModTime = std::filesystem::last_write_time(f, ec);
            }
            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            reloadShaderAsset_(iter->second);
            return iter->second.shader;
        } else {
            asset.shader = LoadShader(nullptr, f.string().c_str());
            auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
            return iter->second.shader;
        }
    }

    Shader& AssetStore::loadShaderFromMemory(const std::string& id, const char* vertSrc, const char* fragSrc) {
        const auto it = shaderAssets_.find(id);
        if (it != shaderAssets_.end())
            return it->second.shader;

        ShaderAsset asset;
        asset.shader = LoadShaderFromMemory(vertSrc, fragSrc);
        asset.hotReload = false;

        auto [iter, _] = shaderAssets_.emplace(id, std::move(asset));
        return iter->second.shader;
    }

    Shader& AssetStore::shader(const std::string& id) {
        return shaderAssets_.at(id).shader;
    }

    ShaderAsset& AssetStore::shaderAsset(const std::string& id) {
        return shaderAssets_.at(id);
    }

    void AssetStore::hotReload(const bool enable) {
        hotReload_ = enable;
    }

    bool AssetStore::hotReload() const { return hotReload_; }

    void AssetStore::setShaderReloadCallback(ShaderReloadCallback cb) {
        shaderReloadCallback_ = std::move(cb);
    }

    void AssetStore::unloadAll() {
        for (const auto& asset : shaderAssets_ | std::views::values) {
            if (asset.shader.id != 0) {
                UnloadShader(asset. shader);
            }
        }
        shaderAssets_.clear();

        for (const auto& kv : fonts_) {
            UnloadFont(kv.second);
        }
        fonts_.clear();
        for (const auto& kv : textures_) {
            UnloadTexture(kv.second);
        }
        textures_.clear();
    }

    bool AssetStore::reloadShaderAsset_(ShaderAsset& asset) const {
        asset.hasError = false;
        asset.errorMessage.clear();

        std::string vertSrc, fragSrc;
        const char* vertPtr = nullptr;
        const char* fragPtr = nullptr;

        if (! asset.vertPath.empty()) {
            if (! readFile_(asset.vertPath, vertSrc)) {
                asset.hasError = true;
                asset.errorMessage = "Failed to read vertex shader file: " + asset.vertPath.string();
                return false;
            }
            vertPtr = vertSrc.c_str();
        }

        if (! asset.fragPath.empty()) {
            if (! readFile_(asset.fragPath, fragSrc)) {
                asset.hasError = true;
                asset.errorMessage = "Failed to read fragment shader file: " + asset.fragPath.string();
            }
            fragPtr = fragSrc.c_str();
        }

        const auto newShader = LoadShaderFromMemory(vertPtr, fragPtr);
        if (newShader.id == 0 || !IsShaderValid(newShader)) {
            asset.hasError = true;
            asset.errorMessage = "Failed to compile shader: " + asset.vertPath.string() + ", " + asset.fragPath.string();
            return false;
        }

        if (asset.shader.id != 0) {
            UnloadShader(asset.shader);
        }

        asset.shader = newShader;
        asset.reloadCount++;

        return true;
    }

    bool AssetStore::readFile_(const std::filesystem::path& path, std::string& out) const {
        const std::ifstream file(path);
        if (!file)
            return false;
        std::ostringstream ss;
        ss << file.rdbuf();
        out = ss.str();
        return true;
    }
}
