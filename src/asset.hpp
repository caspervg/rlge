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
        void unloadAll();

    private:
        std::unordered_map<std::string, Texture2D> textures_;
        std::filesystem::path assetRoot_{std::filesystem::current_path()};
    };
}
