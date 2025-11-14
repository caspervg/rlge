#pragma once
#include <fstream>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "entity.hpp"
#include "raylib.h"
#include "transformer.hpp"

namespace rlge {
    class Tilemap : public Entity {
    public:
        Tilemap(Scene& scene,
                Texture2D& tex,
                int tileW,
                int tileH,
                int mapW,
                int mapH,
                std::vector<int> tiles)
            : Entity(scene),
              texture_(tex),
              tw_(tileW),
              th_(tileH),
              width_(mapW),
              height_(mapH),
              data_(std::move(tiles))
        {
            add<Transform>();
        }

        static Tilemap& loadFromFile(Scene& scene,
                                     Texture2D& tex,
                                     const std::string& path,
                                     int tileW,
                                     int tileH)
        {
            std::ifstream f(path);
            auto w = 0, h = 0;
            f >> w >> h;
            std::vector<int> tiles;
            tiles.reserve(w * h);

            for (auto y = 0; y < h; ++y) {
                for (auto x = 0; x < w; ++x) {
                    int v;
                    f >> v;
                    tiles.push_back(v);
                }
            }

            return scene.spawn<Tilemap>(tex, tileW, tileH, w, h, std::move(tiles));
        }

        void draw() override {
            const auto* tr = get<Transform>();
            const Vector2 offset = tr ? tr->position : Vector2{0, 0};

            for (auto y = 0; y < height_; ++y) {
                for (auto x = 0; x < width_; ++x) {
                    const int idx = data_[y * width_ + x];
                    if (idx < 0)
                        continue;

                    Rectangle src{
                        static_cast<float>(idx * tw_),
                        0.0f,
                        static_cast<float>(tw_),
                        static_cast<float>(th_)
                    };
                    Vector2 pos{
                        offset.x + x * tw_,
                        offset.y + y * th_
                    };

                    scene().renderer().submitBackground(
                        pos.y,
                        [this, src, pos]() {
                            DrawTextureRec(texture_, src, pos, WHITE);
                        });
                }
            }
        }

    private:
        Texture2D& texture_;
        int tw_;
        int th_;
        int width_;
        int height_;
        std::vector<int> data_;
    };
}
