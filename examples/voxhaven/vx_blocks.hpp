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
        GoldOre,
        Glowstone,
        Lantern,
        Bricks,
        Gravel,
        Clay,
        Ice,
        Count
    };

    constexpr int kBlockCount = static_cast<int>(Block::Count);

    // Atlas is an 8x4 grid of 16x16 tiles.
    constexpr int kTilePx = 16;
    constexpr int kAtlasCols = 8;
    constexpr int kAtlasRows = 4;

    // Footstep / mining sound family.
    enum class SoundGroup : std::uint8_t {
        None,
        Grass,
        Stone,
        Wood,
        Sand,
        Glass,
        Snow,
        Liquid
    };

    struct BlockInfo {
        const char* name;
        bool solid;                     // collides with the player
        bool opaque;                    // hides neighboring faces completely
        bool cutout;                    // alpha-tested texture (leaves, glass)
        float hardness;                 // seconds to break; negative = unbreakable
        int tileTop;                    // atlas tile indices
        int tileSide;
        int tileBottom;
        Color mapColor;                 // debris particles / UI accents
        std::uint8_t lightEmission;     // 0..15, light this block radiates
        std::uint8_t lightOpacity;      // light lost passing through (0 = clear, 15 = blocks all)
        SoundGroup sound;
        Block drop;                     // what mining yields (Air = nothing)
    };

    const BlockInfo& blockInfo(Block b);

    // Every block the player can obtain / place, in creative-menu order.
    constexpr std::array<Block, 16> kPlaceable = {
        Block::Grass, Block::Dirt, Block::Stone, Block::Cobble, Block::Planks,
        Block::Wood, Block::Leaves, Block::Sand, Block::Gravel, Block::Clay,
        Block::Bricks, Block::Glass, Block::Snow, Block::Ice, Block::Glowstone,
        Block::Lantern
    };

    // Build the full procedural texture atlas (must be called with a GL context).
    Texture2D buildAtlas(std::uint32_t seed);

    // UV rectangle (in 0..1 atlas space) for a tile index.
    Rectangle tileUV(int tile);

} // namespace vox
