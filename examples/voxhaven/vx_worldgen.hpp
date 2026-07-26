#pragma once
#include <cstdint>

#include "vx_blocks.hpp"

namespace vox {
    struct Chunk;

    enum class Biome : std::uint8_t {
        Ocean,
        Beach,
        Plains,
        Forest,
        Desert,
        Savanna,
        Taiga,
        SnowyPeaks,
        Swamp,
        Count
    };

    const char* biomeName(Biome b);

    namespace worldgen {
        // Fills every block of `chunk` from `seed`. Must be deterministic:
        // the same (seed, chunk coords) always produces identical output,
        // and must not depend on which chunks were generated before it.
        void generateChunk(Chunk& chunk, std::uint32_t seed);

        // Biome at a world column, used by the HUD and by generation.
        Biome biomeAt(std::uint32_t seed, int worldX, int worldZ);

        // Terrain surface height (topmost non-air, ignoring water) at a column.
        int columnHeight(std::uint32_t seed, int worldX, int worldZ);
    } // namespace worldgen

} // namespace vox
