#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "entity.hpp"
#include "raylib.h"

namespace rlge {
    class Scene;
    class Entity;

    class Tilemap : public Entity {
    public:
        struct TileCell {
            int index = -1;
            std::uint32_t flipFlags = 0;
        };

        Tilemap(Scene& scene,
                Texture2D& tex,
                int tileW,
                int tileH,
                int mapW,
                int mapH,
                std::vector<TileCell> tiles,
                int margin = 0,
                int spacing = 0,
                int columns = 0);

        static Tilemap& loadTMX(Scene& scene,
                                 Texture2D& tex,
                                 const std::filesystem::path& path,
                                 const std::string& layerName = "");

        void draw() override;

        // Dimensions in tiles
        int mapWidth() const { return width_; }
        int mapHeight() const { return height_; }

        // Tile size in pixels
        int tileWidth() const { return tw_; }
        int tileHeight() const { return th_; }

    private:
        Texture2D& texture_;
        int tw_;
        int th_;
        int width_;
        int height_;
        std::vector<TileCell> data_;
        int margin_;
        int spacing_;
        int columns_;
    };
}
