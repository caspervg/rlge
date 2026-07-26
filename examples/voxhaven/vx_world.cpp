#include "vx_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

#include "vx_noise.hpp"
#include "vx_worldgen.hpp"

namespace vox {

    namespace {
        constexpr const char* kMagic = "VOXH";
        constexpr std::uint32_t kSaveVersion = 1;

        int floorDiv(const int a, const int b) {
            return static_cast<int>(std::floor(static_cast<float>(a) / static_cast<float>(b)));
        }

        int mod(const int a, const int b) {
            const int r = a % b;
            return r < 0 ? r + b : r;
        }
    } // namespace

    std::uint64_t packCoord(const int x, const int y, const int z) {
        // 26 bits x | 12 bits y | 26 bits z (biased to stay positive)
        const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x + (1 << 25))) & 0x3FFFFFFULL;
        const auto uy = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) & 0xFFFULL;
        const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z + (1 << 25))) & 0x3FFFFFFULL;
        return (ux << 38) | (uy << 26) | uz;
    }

    // -------------------------------------------------------------- Chunk

    Block Chunk::at(const int lx, const int y, const int lz) const {
        if (y < 0 || y >= cfg.worldHeight)
            return Block::Air;
        const int idx = (y * cfg.chunkSize + lz) * cfg.chunkSize + lx;
        return static_cast<Block>(blocks[static_cast<std::size_t>(idx)]);
    }

    void Chunk::set(const int lx, const int y, const int lz, const Block b) {
        if (y < 0 || y >= cfg.worldHeight)
            return;
        const int idx = (y * cfg.chunkSize + lz) * cfg.chunkSize + lx;
        blocks[static_cast<std::size_t>(idx)] = static_cast<std::uint8_t>(b);
    }

    // -------------------------------------------------------------- World

    World::World(const std::string& savePath) :
        savePath_(savePath) {
        seed_ = static_cast<std::uint32_t>(GetRandomValue(1, 0x7FFFFFFF));
        load(); // may overwrite seed_ with the saved one
    }

    World::~World() {
        for (auto& [key, chunk] : chunks_) {
            if (chunk.hasMesh) {
                UnloadMesh(chunk.opaqueMesh);
                UnloadMesh(chunk.waterMesh);
                chunk.hasMesh = false;
            }
        }
    }

    void World::update(const Vector3 playerPos) {
        const int pcx = floorDiv(static_cast<int>(std::floor(playerPos.x)), cfg.chunkSize);
        const int pcz = floorDiv(static_cast<int>(std::floor(playerPos.z)), cfg.chunkSize);

        // Generate missing chunks nearest-first, limited per frame.
        int genBudget = cfg.genPerFrame;
        for (int r = 0; r <= settings.viewRadius && genBudget > 0; ++r) {
            for (int dz = -r; dz <= r && genBudget > 0; ++dz) {
                for (int dx = -r; dx <= r && genBudget > 0; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r)
                        continue; // ring only
                    const ChunkKey key{pcx + dx, pcz + dz};
                    if (chunks_.contains(key))
                        continue;
                    Chunk chunk;
                    chunk.key = key;
                    chunk.blocks.assign(
                        static_cast<std::size_t>(cfg.chunkSize) * cfg.chunkSize * cfg.worldHeight, 0);
                    generateChunk(chunk);
                    applyEditsTo(chunk);
                    chunks_.emplace(key, std::move(chunk));
                    // Neighbors need remeshing so border faces update.
                    markDirty((pcx + dx) * cfg.chunkSize - 1, (pcz + dz) * cfg.chunkSize);
                    markDirty((pcx + dx + 1) * cfg.chunkSize, (pcz + dz) * cfg.chunkSize);
                    markDirty((pcx + dx) * cfg.chunkSize, (pcz + dz) * cfg.chunkSize - 1);
                    markDirty((pcx + dx) * cfg.chunkSize, (pcz + dz + 1) * cfg.chunkSize);
                    genBudget--;
                }
            }
        }

        // Drop far chunks (frees GPU meshes).
        std::vector<ChunkKey> toDrop;
        for (auto& [key, chunk] : chunks_) {
            if (std::max(std::abs(key.cx - pcx), std::abs(key.cz - pcz)) > settings.unloadRadius) {
                toDrop.push_back(key);
            }
        }
        for (const auto& key : toDrop) {
            auto it = chunks_.find(key);
            if (it != chunks_.end()) {
                if (it->second.hasMesh) {
                    UnloadMesh(it->second.opaqueMesh);
                    UnloadMesh(it->second.waterMesh);
                }
                chunks_.erase(it);
            }
        }

        pendingMeshCount_ = 0;
        for (auto& [key, chunk] : chunks_) {
            if (chunk.meshDirty)
                pendingMeshCount_++;
        }
    }

    Block World::block(const int x, const int y, const int z) const {
        if (y < 0 || y >= cfg.worldHeight)
            return Block::Air;
        const ChunkKey key{floorDiv(x, cfg.chunkSize), floorDiv(z, cfg.chunkSize)};
        const auto it = chunks_.find(key);
        if (it == chunks_.end())
            return Block::Stone; // ungenerated area acts solid so nothing falls out of the world
        return it->second.at(mod(x, cfg.chunkSize), y, mod(z, cfg.chunkSize));
    }

    void World::setBlock(const int x, const int y, const int z, const Block b) {
        if (y < 1 || y >= cfg.worldHeight) // keep bedrock floor intact
            return;
        const ChunkKey key{floorDiv(x, cfg.chunkSize), floorDiv(z, cfg.chunkSize)};
        const auto it = chunks_.find(key);
        if (it == chunks_.end())
            return;
        it->second.set(mod(x, cfg.chunkSize), y, mod(z, cfg.chunkSize), b);
        edits_[packCoord(x, y, z)] = static_cast<std::uint8_t>(b);

        // Dirty this chunk and any neighbor sharing the touched face column.
        markDirty(x, z);
        if (mod(x, cfg.chunkSize) == 0) markDirty(x - 1, z);
        if (mod(x, cfg.chunkSize) == cfg.chunkSize - 1) markDirty(x + 1, z);
        if (mod(z, cfg.chunkSize) == 0) markDirty(x, z - 1);
        if (mod(z, cfg.chunkSize) == cfg.chunkSize - 1) markDirty(x, z + 1);
    }

    bool World::solidAt(const int x, const int y, const int z) const {
        return blockInfo(block(x, y, z)).solid;
    }

    int World::surfaceHeight(const int x, const int z) const {
        for (int y = cfg.worldHeight - 1; y > 0; --y) {
            if (blockInfo(block(x, y, z)).solid)
                return y;
        }
        return 1;
    }

    void World::markDirty(const int x, const int z) {
        const ChunkKey key{floorDiv(x, cfg.chunkSize), floorDiv(z, cfg.chunkSize)};
        const auto it = chunks_.find(key);
        if (it != chunks_.end()) {
            it->second.meshDirty = true;
        }
    }

    // --------------------------------------------------------- Generation

    void World::generateChunk(Chunk& chunk) const {
        worldgen::generateChunk(chunk, seed_);
    }

    void World::applyEditsTo(Chunk& chunk) const {
        const int baseX = chunk.key.cx * cfg.chunkSize;
        const int baseZ = chunk.key.cz * cfg.chunkSize;
        for (const auto& [key, value] : edits_) {
            // Unpack and check membership; the edit map stays small (player-made).
            const int x = static_cast<int>((key >> 38) & 0x3FFFFFFULL) - (1 << 25);
            const int y = static_cast<int>((key >> 26) & 0xFFFULL);
            const int z = static_cast<int>(key & 0x3FFFFFFULL) - (1 << 25);
            if (x < baseX || x >= baseX + cfg.chunkSize || z < baseZ || z >= baseZ + cfg.chunkSize)
                continue;
            chunk.set(x - baseX, y, z - baseZ, static_cast<Block>(value));
        }
    }

    // ------------------------------------------------------------ Raycast

    std::optional<RayHit> World::raycast(const Vector3 origin, const Vector3 dir, const float maxDist) const {
        int x = static_cast<int>(std::floor(origin.x));
        int y = static_cast<int>(std::floor(origin.y));
        int z = static_cast<int>(std::floor(origin.z));

        const int stepX = dir.x > 0.0f ? 1 : -1;
        const int stepY = dir.y > 0.0f ? 1 : -1;
        const int stepZ = dir.z > 0.0f ? 1 : -1;

        const auto tDelta = [](const float d) {
            return d != 0.0f ? std::fabs(1.0f / d) : 1e30f;
        };
        const float tDeltaX = tDelta(dir.x);
        const float tDeltaY = tDelta(dir.y);
        const float tDeltaZ = tDelta(dir.z);

        const auto tInit = [](const float o, const float d, const int i, const int step) {
            if (d == 0.0f)
                return 1e30f;
            const float boundary = step > 0 ? static_cast<float>(i) + 1.0f : static_cast<float>(i);
            return (boundary - o) / d;
        };
        float tMaxX = tInit(origin.x, dir.x, x, stepX);
        float tMaxY = tInit(origin.y, dir.y, y, stepY);
        float tMaxZ = tInit(origin.z, dir.z, z, stepZ);

        int nx = 0, ny = 0, nz = 0;
        float t = 0.0f;
        while (t <= maxDist) {
            const Block b = block(x, y, z);
            if (b != Block::Air && b != Block::Water) {
                return RayHit{x, y, z, nx, ny, nz, b};
            }
            if (tMaxX < tMaxY && tMaxX < tMaxZ) {
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                nx = -stepX; ny = 0; nz = 0;
            } else if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                nx = 0; ny = -stepY; nz = 0;
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                nx = 0; ny = 0; nz = -stepZ;
            }
        }
        return std::nullopt;
    }

    // -------------------------------------------------------- Persistence

    void World::save() const {
        std::ofstream out(savePath_, std::ios::binary | std::ios::trunc);
        if (!out)
            return;
        out.write(kMagic, 4);
        const std::uint32_t version = kSaveVersion;
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&seed_), sizeof(seed_));
        const auto count = static_cast<std::uint32_t>(edits_.size());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [key, value] : edits_) {
            out.write(reinterpret_cast<const char*>(&key), sizeof(key));
            out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        TraceLog(LOG_INFO, "VOXHAVEN: saved %u edits to %s", count, savePath_.c_str());
    }

    void World::load() {
        std::ifstream in(savePath_, std::ios::binary);
        if (!in)
            return;
        char magic[4] = {};
        in.read(magic, 4);
        if (std::string_view(magic, 4) != kMagic)
            return;
        std::uint32_t version = 0;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != kSaveVersion)
            return;
        in.read(reinterpret_cast<char*>(&seed_), sizeof(seed_));
        std::uint32_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        for (std::uint32_t i = 0; i < count && in; ++i) {
            std::uint64_t key = 0;
            std::uint8_t value = 0;
            in.read(reinterpret_cast<char*>(&key), sizeof(key));
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            edits_[key] = value;
        }
        TraceLog(LOG_INFO, "VOXHAVEN: loaded %zu edits (seed %u)", edits_.size(), seed_);
    }

} // namespace vox
