#pragma once
#include <string>
#include <vector>

#include "entity.hpp"
#include "raylib.h"

namespace rlge {
    class Scene;
    class Entity;

    class Tilemap : public Entity {
    public:
        Tilemap(Scene& scene,
                Texture2D& tex,
                int tileW,
                int tileH,
                int mapW,
                int mapH,
                std::vector<int> tiles);

        static Tilemap& loadFromFile(Scene& scene,
                                     Texture2D& tex,
                                     const std::string& path,
                                     int tileW,
                                     int tileH);

        void draw() override;

    private:
        Texture2D& texture_;
        int tw_;
        int th_;
        int width_;
        int height_;
        std::vector<int> data_;
    };
}
