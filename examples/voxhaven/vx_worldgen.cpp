#include "vx_worldgen.hpp"

#include <algorithm>
#include <cmath>

#include "vx_config.hpp"
#include "vx_noise.hpp"
#include "vx_world.hpp"

namespace vox {

    const char* biomeName(const Biome b) {
        switch (b) {
        case Biome::Ocean: return "ocean";
        case Biome::Beach: return "beach";
        case Biome::Plains: return "plains";
        case Biome::Forest: return "forest";
        case Biome::Desert: return "desert";
        case Biome::Savanna: return "savanna";
        case Biome::Taiga: return "taiga";
        case Biome::SnowyPeaks: return "snowy peaks";
        case Biome::Swamp: return "swamp";
        default: return "unknown";
        }
    }

    namespace worldgen {

        int columnHeight(const std::uint32_t seed, const int worldX, const int worldZ) {
            const auto fx = static_cast<float>(worldX);
            const auto fz = static_cast<float>(worldZ);
            const float relief = noise::fbm2(seed, fx * 0.004f, fz * 0.004f, 4);
            const float hills = noise::fbm2(seed + 9, fx * 0.02f, fz * 0.02f, 4);
            const float h = 26.0f + relief * 34.0f + (hills - 0.5f) * 18.0f;
            return std::clamp(static_cast<int>(h), 4, cfg.worldHeight - 8);
        }

        Biome biomeAt(const std::uint32_t seed, const int worldX, const int worldZ) {
            const auto fx = static_cast<float>(worldX);
            const auto fz = static_cast<float>(worldZ);
            const float temp = noise::fbm2(seed + 21, fx * 0.003f, fz * 0.003f, 3);
            const int h = columnHeight(seed, worldX, worldZ);
            if (h < cfg.seaLevel - 1) return Biome::Ocean;
            if (h <= cfg.seaLevel + 1) return Biome::Beach;
            if (temp > 0.62f) return Biome::Desert;
            if (temp < 0.34f) return Biome::Taiga;
            return Biome::Plains;
        }

        void generateChunk(Chunk& chunk, const std::uint32_t seed) {
            const int baseX = chunk.key.cx * cfg.chunkSize;
            const int baseZ = chunk.key.cz * cfg.chunkSize;

            for (int lz = 0; lz < cfg.chunkSize; ++lz) {
                for (int lx = 0; lx < cfg.chunkSize; ++lx) {
                    const int wx = baseX + lx;
                    const int wz = baseZ + lz;
                    const auto fx = static_cast<float>(wx);
                    const auto fz = static_cast<float>(wz);

                    const float temp = noise::fbm2(seed + 21, fx * 0.003f, fz * 0.003f, 3);
                    const int surface = columnHeight(seed, wx, wz);
                    const bool desert = temp > 0.62f;
                    const bool snowy = temp < 0.34f;

                    for (int y = 0; y < cfg.worldHeight; ++y) {
                        Block b = Block::Air;
                        if (y == 0) {
                            b = Block::Bedrock;
                        } else if (y < surface - 3) {
                            b = Block::Stone;
                            const float ore = noise::rand01(seed + 31, wx, y, wz);
                            if (y < 40 && ore > 0.982f) b = Block::CoalOre;
                            else if (y < 24 && ore < 0.008f) b = Block::IronOre;
                            const float cave = noise::fbm3(seed + 47, fx * 0.045f, y * 0.06f, fz * 0.045f, 3);
                            if (cave > 0.62f && y > 2) b = Block::Air;
                        } else if (y < surface) {
                            b = desert ? Block::Sand : Block::Dirt;
                        } else if (y == surface) {
                            if (desert) b = Block::Sand;
                            else if (surface <= cfg.seaLevel + 1) b = Block::Sand;
                            else if (snowy || surface > 58) b = Block::Snow;
                            else b = Block::Grass;
                        } else if (y <= cfg.seaLevel) {
                            b = Block::Water;
                        }
                        chunk.set(lx, y, lz, b);
                    }

                    // Trees on grass, deterministic per column.
                    if (chunk.at(lx, surface, lz) == Block::Grass &&
                        lx >= 2 && lx < cfg.chunkSize - 2 && lz >= 2 && lz < cfg.chunkSize - 2) {
                        const float forest = noise::fbm2(seed + 63, fx * 0.015f, fz * 0.015f, 3);
                        const float roll = noise::rand01(seed + 71, wx, wz);
                        if (forest > 0.52f && roll > 0.975f) {
                            const int trunkH = 4 + static_cast<int>(noise::rand01(seed + 72, wx, wz) * 3.0f);
                            for (int t = 1; t <= trunkH; ++t) {
                                chunk.set(lx, surface + t, lz, Block::Wood);
                            }
                            for (int dy = trunkH - 2; dy <= trunkH + 2; ++dy) {
                                for (int dz2 = -2; dz2 <= 2; ++dz2) {
                                    for (int dx2 = -2; dx2 <= 2; ++dx2) {
                                        if (std::abs(dx2) + std::abs(dz2) + std::abs(dy - trunkH) > 3)
                                            continue;
                                        const int ty = surface + dy;
                                        if (chunk.at(lx + dx2, ty, lz + dz2) == Block::Air) {
                                            chunk.set(lx + dx2, ty, lz + dz2, Block::Leaves);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    } // namespace worldgen
} // namespace vox
