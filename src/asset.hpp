#pragma once
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    class AssetStore final {
    public:
        AssetStore() = default;
        ~AssetStore();

        AssetStore(const AssetStore&) = delete;
        AssetStore& operator=(const AssetStore&) = delete;

        Texture2D& loadTexture(const std::string& id, const std::string& path);
        Texture2D& texture(const std::string& id);
        void unloadAll();

    private:
        std::unordered_map<std::string, Texture2D> textures_;
    };
}
