#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    using ShaderReloadCallback = std::function<void(const std::string& id, bool success)>;

    struct ShaderAsset {
        Shader shader{};
        std::filesystem::path vertPath;
        std::filesystem::path fragPath;
        std::filesystem::file_time_type lastVertModTime;
        std::filesystem::file_time_type lastFragModTime;
        bool hotReload{false};
        bool hasError{false};
        std::string errorMessage;
        int reloadCount{0};
    };

    class AssetStore final {
    public:
        AssetStore() = default;
        ~AssetStore();

        AssetStore(const AssetStore&) = delete;
        AssetStore& operator=(const AssetStore&) = delete;

        // Configure/inspect asset root; used to resolve relative paths.
        void setRoot(std::filesystem::path root);
        [[nodiscard]] const std::filesystem::path& root() const { return assetRoot_; }
        // Join and normalize a path against the root (absolute inputs pass through).
        [[nodiscard]] std::filesystem::path path(std::string_view relative) const;

        Texture2D& loadTexture(const std::string& id, const std::string& path);
        Texture2D& texture(const std::string& id);

        // Fonts
        Font& loadFont(const std::string& id, const std::string& path, int size = 32);
        Font& font(const std::string& id);

        // Shader loading helpers
        Shader& loadShader(const std::string& id, const std::string& vertPath, const std::string& fragPath);
        Shader& loadVertexShader(const std::string& id, const std::string& vertPath);
        Shader& loadFragmentShader(const std::string& id, const std::string& fragPath);
        Shader& loadShaderFromMemory(const std::string& id, const char* vertSrc, const char* fragSrc);
        Shader& shader(const std::string& id);
        [[nodiscard]] ShaderAsset& shaderAsset(const std::string& id);

        void hotReload(bool enable);
        [[nodiscard]] bool hotReload() const;
        void setShaderReloadCallback(ShaderReloadCallback cb);

        void unloadAll();

    private:
        bool reloadShaderAsset_(ShaderAsset& asset) const;
        bool readFile_(const std::filesystem::path& path, std::string& out) const;

    private:
        std::unordered_map<std::string, Texture2D> textures_;
        std::unordered_map<std::string, Font> fonts_;
        std::unordered_map<std::string, ShaderAsset> shaderAssets_;
        std::filesystem::path assetRoot_{std::filesystem::current_path()};

        bool hotReload_{false};
        float hotReloadInterval_ = 0.5f;
        float hotReloadLastCheck_ = 0.0f;
        ShaderReloadCallback shaderReloadCallback_{nullptr};
    };
}
