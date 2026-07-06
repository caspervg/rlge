#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

#include "vx_blocks.hpp"
#include "vx_config.hpp"

namespace vox {

    struct ChunkKey {
        int cx;
        int cz;
        bool operator==(const ChunkKey&) const = default;
    };

    struct ChunkKeyHash {
        std::size_t operator()(const ChunkKey& k) const noexcept {
            const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(k.cx));
            const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(k.cz));
            return std::hash<std::uint64_t>{}((ux << 32) | uz);
        }
    };

    struct Chunk {
        ChunkKey key{};
        std::vector<std::uint8_t> blocks; // size = chunkSize * worldHeight * chunkSize
        bool meshDirty = true;
        bool hasMesh = false;
        Mesh opaqueMesh{};
        Mesh waterMesh{};

        [[nodiscard]] Block at(int lx, int y, int lz) const;
        void set(int lx, int y, int lz, Block b);
    };

    struct RayHit {
        int x, y, z;       // hit block
        int nx, ny, nz;    // face normal (unit axis)
        Block block;
    };

    // Chunked voxel world: procedural generation + player edits + persistence.
    class World {
    public:
        explicit World(const std::string& savePath);
        ~World();

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // Stream chunks around the player; obeys per-frame budgets.
        void update(Vector3 playerPos);

        [[nodiscard]] Block block(int x, int y, int z) const;
        void setBlock(int x, int y, int z, Block b); // records the edit + dirties meshes

        [[nodiscard]] bool solidAt(int x, int y, int z) const;
        [[nodiscard]] int surfaceHeight(int x, int z) const; // topmost solid y

        // Amanatides & Woo voxel traversal.
        [[nodiscard]] std::optional<RayHit> raycast(Vector3 origin, Vector3 dir, float maxDist) const;

        [[nodiscard]] std::uint32_t seed() const { return seed_; }
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>& chunks() { return chunks_; }
        [[nodiscard]] int pendingMeshes() const { return pendingMeshCount_; }

        void save() const;

        // Set by the scene so freshly loaded/edited chunks get remeshed.
        int meshBudgetUsedThisFrame = 0;

    private:
        void generateChunk(Chunk& chunk) const;
        void applyEditsTo(Chunk& chunk) const;
        void markDirty(int x, int z);
        void load();

        std::uint32_t seed_ = 1337;
        std::string savePath_;
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> chunks_;
        std::unordered_map<std::uint64_t, std::uint8_t> edits_; // packed coord -> block
        int pendingMeshCount_ = 0;

        friend class Mesher;
    };

    // Packs a world coordinate into an edit key.
    std::uint64_t packCoord(int x, int y, int z);

} // namespace vox
