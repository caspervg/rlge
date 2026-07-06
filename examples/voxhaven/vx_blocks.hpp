#pragma once
#include <array>
#include <cstdint>

#include "raylib.h"

namespace vox {

    enum class Block : std::uint8_t {
        Air = 0,
        Grass,
        Dirt,
        Stone,
        Cobble,
        Sand,
        Snow,
        Wood,
        Leaves,
        Planks,
        Glass,
        Water,
        Bedrock,
        CoalOre,
        IronOre,
        Glowstone,
        Count
    };

    constexpr int kBlockCount = static_cast<int>(Block::Count);

    // Atlas is a 8x4 grid of 16x16 tiles.
    constexpr int kTilePx = 16;
    constexpr int kAtlasCols = 8;
    constexpr int kAtlasRows = 4;

    struct BlockInfo {
        const char* name;
        bool solid;        // collides with the player
        bool opaque;       // hides neighboring faces completely
        bool cutout;       // alpha-tested texture (leaves, glass)
        float hardness;    // seconds to break
        int tileTop;       // atlas tile indices
        int tileSide;
        int tileBottom;
        Color mapColor;    // debris particles / UI accents
    };

    const BlockInfo& blockInfo(Block b);

    // Blocks the player can put on the hotbar.
    constexpr std::array<Block, 9> kHotbar = {
        Block::Grass, Block::Dirt, Block::Stone, Block::Cobble, Block::Planks,
        Block::Wood, Block::Glass, Block::Sand, Block::Glowstone
    };

    // Build the full procedural texture atlas (must be called with a GL context).
    Texture2D buildAtlas(std::uint32_t seed);

    // UV rectangle (in 0..1 atlas space) for a tile index.
    Rectangle tileUV(int tile);

} // namespace vox
