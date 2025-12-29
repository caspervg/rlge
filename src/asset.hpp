#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "sprite_atlas.hpp"

namespace rlge {
    struct ShaderHandle {
        std::uint32_t value{0};
        explicit constexpr operator bool() const noexcept { return value != 0; }
        constexpr bool operator==(const ShaderHandle& other) const noexcept { return value == other.value; }
    };

    constexpr ShaderHandle InvalidShaderHandle{0};

    struct ShaderHandleHash {
        std::size_t operator()(ShaderHandle h) const noexcept { return static_cast<std::size_t>(h.value); }
    };

    using ShaderReloadCallback = std::function<void(ShaderHandle handle, bool success)>;

    struct ShaderAsset {
        std::string name;
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
        ShaderHandle loadShader(const std::string& id, const std::string& vertPath, const std::string& fragPath);
        ShaderHandle loadVertexShader(const std::string& id, const std::string& vertPath);
        ShaderHandle loadFragmentShader(const std::string& id, const std::string& fragPath);
        ShaderHandle loadShaderFromMemory(const std::string& id, const char* vertSrc, const char* fragSrc);
        Shader& shader(ShaderHandle handle);
        [[nodiscard]] ShaderAsset& shaderAsset(ShaderHandle handle);
        [[nodiscard]] const ShaderAsset& shaderAsset(ShaderHandle handle) const;
        [[nodiscard]] const std::unordered_map<ShaderHandle, ShaderAsset, ShaderHandleHash>& shaderAssets() const { return shaderAssets_; }

        template <typename StateEnum>
        SpriteAtlas<StateEnum>& loadAtlas(const AtlasSpec<StateEnum>& spec);

        template <typename StateEnum>
        SpriteAtlas<StateEnum>& atlas(const std::string& id);

        // Poll shaders for changes when hotReload is enabled.
        void update(float dt);

        void hotReload(bool enable);
        [[nodiscard]] bool hotReload() const;
        void setShaderReloadCallback(ShaderReloadCallback cb);
        void addShaderReloadListener(ShaderReloadCallback cb);

        // Debug overlay for shader assets (reload counts/errors)
        void debugOverlay();
        void unloadAll();

    private:
        bool reloadShaderAsset_(ShaderAsset& asset) const;
        bool readFile_(const std::filesystem::path& path, std::string& out) const;

    private:
        std::unordered_map<std::string, Texture2D> textures_;
        std::unordered_map<std::string, Font> fonts_;
        std::unordered_map<ShaderHandle, ShaderAsset, ShaderHandleHash> shaderAssets_;
        std::unordered_map<std::string, ShaderHandle> shaderNameToHandle_;
        std::unordered_map<std::string, std::unique_ptr<SpriteAtlasBase>> atlases_;
        std::filesystem::path assetRoot_{std::filesystem::current_path()};

        std::uint32_t nextShaderHandle_{1};
        bool hotReload_{false};
        float hotReloadInterval_ = 0.5f;
        float hotReloadLastCheck_ = 0.0f;
        ShaderReloadCallback shaderReloadCallback_{nullptr};
        std::vector<ShaderReloadCallback> shaderReloadListeners_;
    };
}
