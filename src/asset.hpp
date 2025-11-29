#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

#include "raylib.h"

namespace rlge {
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

        void unloadAll();

    private:
        std::unordered_map<std::string, Texture2D> textures_;
        std::unordered_map<std::string, Font> fonts_;
        std::unordered_map<std::string, Shader> shaders_;
        std::filesystem::path assetRoot_{std::filesystem::current_path()};
    };
}
