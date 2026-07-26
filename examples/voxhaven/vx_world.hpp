#pragma once
#include <cstddef>
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
        // One byte per block: skylight in the high nibble, block light in the low
        // nibble. Packing both halves keeps the mesher's padded snapshot copy to a
        // single linear pass instead of two.
        std::vector<std::uint8_t> light;
        bool meshDirty = true;
        bool hasMesh = false;
        bool lightDirty = true; // needs a full light::computeChunk pass
        Mesh opaqueMesh{};
        Mesh waterMesh{};

        // Tight vertical span of non-air blocks; maxY < minY means "all air".
        // Tracked so the renderer can frustum-cull against a real AABB instead of
        // the full worldHeight column.
        int minY = 0;
        int maxY = -1;

        [[nodiscard]] static int index(const int lx, const int y, const int lz) {
            return (y * cfg.chunkSize + lz) * cfg.chunkSize + lx;
        }

        // Sizes blocks/light for the configured world shape and resets the extent.
        void allocate();

        [[nodiscard]] Block at(int lx, int y, int lz) const;
        void set(int lx, int y, int lz, Block b);

        [[nodiscard]] std::uint8_t packedLightAt(int lx, int y, int lz) const;
        [[nodiscard]] int skyAt(int lx, int y, int lz) const;
        [[nodiscard]] int blockLightAt(int lx, int y, int lz) const;
        void setSkyAt(int lx, int y, int lz, int v);
        void setBlockLightAt(int lx, int y, int lz, int v);

        void expandExtent(int y);   // cheap incremental widen after placing a block
        void recomputeExtent();     // full rescan; used after generation / removals
        [[nodiscard]] bool emptyColumn() const { return maxY < minY; }

        // Tight world-space AABB of the solid part of this chunk.
        [[nodiscard]] BoundingBox bounds() const;
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

        // Light lookups in world space; 0 outside loaded chunks.
        [[nodiscard]] int skyLight(int x, int y, int z) const;
        [[nodiscard]] int blockLight(int x, int y, int z) const;

        [[nodiscard]] Chunk* chunkAt(int x, int z);
        [[nodiscard]] const Chunk* chunkAt(int x, int z) const;
        [[nodiscard]] Chunk* chunkByKey(ChunkKey key);
        [[nodiscard]] const Chunk* chunkByKey(ChunkKey key) const;

        // Amanatides & Woo voxel traversal.
        [[nodiscard]] std::optional<RayHit> raycast(Vector3 origin, Vector3 dir, float maxDist) const;

        [[nodiscard]] std::uint32_t seed() const { return seed_; }
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>& chunks() { return chunks_; }
        [[nodiscard]] const std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>& chunks() const {
            return chunks_;
        }
        [[nodiscard]] int pendingMeshes() const { return pendingMeshCount_; }
        [[nodiscard]] int pendingLight() const { return pendingLightCount_; }
        [[nodiscard]] std::size_t editCount() const;

        // Force a chunk (and its border neighbours) to remesh.
        void markDirty(int x, int z);

        void save() const;

        // Set by the scene so freshly loaded/edited chunks get remeshed.
        int meshBudgetUsedThisFrame = 0;

    private:
        void generateChunk(Chunk& chunk) const;
        void applyEditsTo(Chunk& chunk) const;
        void load();

        std::uint32_t seed_ = 1337;
        std::string savePath_;
        std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> chunks_;
        // Edits bucketed per chunk so applying them to a new chunk is O(edits in
        // that chunk) instead of O(all edits). The on-disk format is unchanged:
        // the buckets are flattened back to packed-coord/block pairs on save.
        std::unordered_map<ChunkKey, std::unordered_map<std::uint64_t, std::uint8_t>, ChunkKeyHash>
            edits_;
        int pendingMeshCount_ = 0;
        int pendingLightCount_ = 0;

        friend class Mesher;
    };

    // Packs a world coordinate into an edit key.
    std::uint64_t packCoord(int x, int y, int z);
    void unpackCoord(std::uint64_t packed, int& x, int& y, int& z);

    // Floor-division helpers shared by the world, mesher and light code.
    [[nodiscard]] int floorDivInt(int a, int b);
    [[nodiscard]] int modInt(int a, int b);

    // Six-plane frustum test against the camera's view-projection matrix.
    // `aspect` is viewport width / height.
    [[nodiscard]] bool chunkInFrustum(const Camera3D& cam, float aspect, const BoundingBox& box);

} // namespace vox
