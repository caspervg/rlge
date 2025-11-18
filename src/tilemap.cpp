#include "tilemap.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <tuple>

#include <tileson.hpp>

#include "scene.hpp"
#include "transformer.hpp"
#include "render_queue.hpp"

namespace rlge {
    Tilemap::Tilemap(Scene& scene,
                     Texture2D& tex,
                     int tileW,
                     int tileH,
                     int mapW,
                     int mapH,
                     std::vector<TileCell> tiles,
                     int margin,
                     int spacing,
                     int columns)
        : Entity(scene)
        , texture_(tex)
        , tw_(tileW)
        , th_(tileH)
        , width_(mapW)
        , height_(mapH)
        , data_(std::move(tiles))
        , margin_(margin)
        , spacing_(spacing)
        , columns_(columns) {
        add<Transform>();
    }

    namespace {
        const tson::Layer* findTileLayer(std::vector<tson::Layer>& layers, const std::string& layerName) {
            for (auto& layer : layers) {
                if (layer.getType() == tson::LayerType::TileLayer) {
                    if (layerName.empty() || layer.getName() == layerName) {
                        return &layer;
                    }
                }

                auto& nested = layer.getLayers();
                if (!nested.empty()) {
                    if (const auto* candidate = findTileLayer(nested, layerName)) {
                        return candidate;
                    }
                }
            }
            return nullptr;
        }
    }

    Tilemap& Tilemap::loadTMX(Scene& scene,
                              Texture2D& tex,
                              const std::filesystem::path& path,
                              const std::string& layerName) {
        tson::Tileson parser;
        auto map = parser.parse(path);
        if (!map || map->getStatus() != tson::ParseStatus::OK) {
            const std::string msg = map ? map->getStatusMessage() : "unknown error";
            throw std::runtime_error("Failed to parse Tiled map '" + path.string() + "': " + msg);
        }

        if (map->getOrientation() != "orthogonal") {
            throw std::runtime_error("Only orthogonal Tiled maps are supported right now");
        }

        if (map->isInfinite()) {
            throw std::runtime_error("Infinite Tiled maps are not supported yet");
        }

        auto& tilesets = map->getTilesets();
        if (tilesets.size() != 1) {
            throw std::runtime_error("Tiled maps with exactly one tileset are supported");
        }

        auto& tileset = tilesets.front();

        auto* tileLayer = findTileLayer(map->getLayers(), layerName);
        if (tileLayer == nullptr) {
            throw std::runtime_error(layerName.empty() ? "No tile layers were found" : "Tile layer '" + layerName + "' was not found");
        }

        const auto layerSize = tileLayer->getSize();
        const int mapW = layerSize.x;
        const int mapH = layerSize.y;
        std::vector<Tilemap::TileCell> tiles(static_cast<size_t>(mapW) * static_cast<size_t>(mapH));

        for (const auto& entry : tileLayer->getTileData()) {
            const auto tile = entry.second;
            if (!tile) {
                continue;
            }

            if (tile->getTileset() != &tileset) {
                throw std::runtime_error("Tiles spanning multiple tilesets are not supported yet");
            }

            const auto [tileX, tileY] = entry.first;
            if (tileX < 0 || tileY < 0 || tileX >= mapW || tileY >= mapH) {
                continue;
            }

            auto& cell = tiles[tileY * mapW + tileX];
            cell.index = static_cast<int>(tile->getGid()) - tileset.getFirstgid();
            cell.flipFlags = static_cast<std::uint32_t>(tile->getFlipFlags());
        }

        int margin = tileset.getMargin();
        int spacing = tileset.getSpacing();
        int columns = tileset.getColumns();
        if (columns <= 0) {
            const auto imageSize = tileset.getImageSize();
            const int pitch = tileset.getTileSize().x + spacing;
            if (pitch > 0) {
                columns = std::max(1, (imageSize.x - margin * 2 + spacing) / pitch);
            }
        }

        if (columns <= 0) {
            throw std::runtime_error("Could not infer tileset column count");
        }

        const int tileW = map->getTileSize().x;
        const int tileH = map->getTileSize().y;

        return scene.spawn<Tilemap>(tex,
                                    tileW,
                                    tileH,
                                    mapW,
                                    mapH,
                                    std::move(tiles),
                                    margin,
                                    spacing,
                                    columns);
    }

    void Tilemap::draw() {
        const auto* tr = get<Transform>();
        const Vector2 offset = tr ? tr->position : Vector2{0, 0};

        constexpr std::uint32_t FLIP_H = 0x80000000u;
        constexpr std::uint32_t FLIP_V = 0x40000000u;
        constexpr std::uint32_t FLIP_D = 0x20000000u;

        auto& rq = scene().rq();

        // Iterate over all tiles; per-view culling happens in the renderer.
        for (auto y = 0; y < height_; ++y) {
            for (auto x = 0; x < width_; ++x) {
                const TileCell& cell = data_[y * width_ + x];
                if (cell.index < 0)
                    continue;

                const int cols = columns_ > 0 ? columns_ : width_;
                const int tileX = cell.index % cols;
                const int tileY = cell.index / cols;
                Rectangle src{
                    static_cast<float>(margin_ + tileX * (tw_ + spacing_)),
                    static_cast<float>(margin_ + tileY * (th_ + spacing_)),
                    static_cast<float>(tw_),
                    static_cast<float>(th_)
                };
                Vector2 pos{
                    offset.x + x * tw_,
                    offset.y + y * th_
                };

                bool flipH = (cell.flipFlags & FLIP_H) != 0;
                bool flipV = (cell.flipFlags & FLIP_V) != 0;
                const bool flipD = (cell.flipFlags & FLIP_D) != 0;

                const Vector2 halfSize{src.width / 2.0f, src.height / 2.0f};
                Vector2 originOffset{halfSize.x, halfSize.y + static_cast<float>(th_) - src.height};
                float rotation = 0.0f;

                if (flipD) {
                    rotation = 90.0f;
                    const bool originalH = flipH;
                    flipH = flipV;
                    flipV = !originalH;

                    const float halfDiff = halfSize.y - halfSize.x;
                    originOffset.x += halfDiff;
                    originOffset.y += halfDiff;
                }

                const float scaleX = flipH ? -1.0f : 1.0f;
                const float scaleY = flipV ? -1.0f : 1.0f;

                Rectangle dest{
                    pos.x + originOffset.x,
                    pos.y + originOffset.y,
                    src.width * scaleX,
                    src.height * scaleY
                };
                const Vector2 origin{halfSize.x, halfSize.y};

                // Use batched sprite submission instead of lambda
                rq.submitSprite(RenderLayer::Background, 0.0f, texture_,
                               src, dest, origin, rotation, WHITE);
            }
        }
    }
}
