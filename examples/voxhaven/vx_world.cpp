#include "vx_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

#include "raymath.h"
#include "rlgl.h"

#include "vx_light.hpp"
#include "vx_noise.hpp"
#include "vx_worldgen.hpp"

namespace vox {

    namespace {
        constexpr const char* kMagic = "VOXH";
        constexpr std::uint32_t kSaveVersion = 1;

        ChunkKey keyOf(const int x, const int z) {
            return ChunkKey{floorDivInt(x, cfg.chunkSize), floorDivInt(z, cfg.chunkSize)};
        }
    } // namespace

    int floorDivInt(const int a, const int b) {
        const int q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    int modInt(const int a, const int b) {
        const int r = a % b;
        return r < 0 ? r + b : r;
    }

    std::uint64_t packCoord(const int x, const int y, const int z) {
        // 26 bits x | 12 bits y | 26 bits z (biased to stay positive)
        const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x + (1 << 25))) & 0x3FFFFFFULL;
        const auto uy = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) & 0xFFFULL;
        const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z + (1 << 25))) & 0x3FFFFFFULL;
        return (ux << 38) | (uy << 26) | uz;
    }

    void unpackCoord(const std::uint64_t packed, int& x, int& y, int& z) {
        x = static_cast<int>((packed >> 38) & 0x3FFFFFFULL) - (1 << 25);
        y = static_cast<int>((packed >> 26) & 0xFFFULL);
        z = static_cast<int>(packed & 0x3FFFFFFULL) - (1 << 25);
    }

    // -------------------------------------------------------------- Chunk

    void Chunk::allocate() {
        const auto count =
            static_cast<std::size_t>(cfg.chunkSize) * cfg.chunkSize * cfg.worldHeight;
        blocks.assign(count, 0);
        light.assign(count, 0);
        minY = 0;
        maxY = -1;
        lightDirty = true;
        meshDirty = true;
    }

    Block Chunk::at(const int lx, const int y, const int lz) const {
        if (y < 0 || y >= cfg.worldHeight)
            return Block::Air;
        return static_cast<Block>(blocks[static_cast<std::size_t>(index(lx, y, lz))]);
    }

    void Chunk::set(const int lx, const int y, const int lz, const Block b) {
        if (y < 0 || y >= cfg.worldHeight)
            return;
        blocks[static_cast<std::size_t>(index(lx, y, lz))] = static_cast<std::uint8_t>(b);
    }

    std::uint8_t Chunk::packedLightAt(const int lx, const int y, const int lz) const {
        if (y < 0 || y >= cfg.worldHeight)
            return 0;
        return light[static_cast<std::size_t>(index(lx, y, lz))];
    }

    int Chunk::skyAt(const int lx, const int y, const int lz) const {
        return packedLightAt(lx, y, lz) >> 4;
    }

    int Chunk::blockLightAt(const int lx, const int y, const int lz) const {
        return packedLightAt(lx, y, lz) & 0x0F;
    }

    void Chunk::setSkyAt(const int lx, const int y, const int lz, const int v) {
        if (y < 0 || y >= cfg.worldHeight)
            return;
        std::uint8_t& cell = light[static_cast<std::size_t>(index(lx, y, lz))];
        cell = static_cast<std::uint8_t>((cell & 0x0F) | ((v & 0x0F) << 4));
    }

    void Chunk::setBlockLightAt(const int lx, const int y, const int lz, const int v) {
        if (y < 0 || y >= cfg.worldHeight)
            return;
        std::uint8_t& cell = light[static_cast<std::size_t>(index(lx, y, lz))];
        cell = static_cast<std::uint8_t>((cell & 0xF0) | (v & 0x0F));
    }

    void Chunk::expandExtent(const int y) {
        if (emptyColumn()) {
            minY = y;
            maxY = y;
            return;
        }
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    void Chunk::recomputeExtent() {
        minY = 0;
        maxY = -1;
        for (int y = cfg.worldHeight - 1; y >= 0; --y) {
            const auto rowBase = static_cast<std::size_t>(index(0, y, 0));
            const auto rowEnd = rowBase + static_cast<std::size_t>(cfg.chunkSize) * cfg.chunkSize;
            const bool any = std::any_of(blocks.begin() + static_cast<std::ptrdiff_t>(rowBase),
                                         blocks.begin() + static_cast<std::ptrdiff_t>(rowEnd),
                                         [](const std::uint8_t v) { return v != 0; });
            if (!any)
                continue;
            if (maxY < 0)
                maxY = y;
            minY = y;
        }
        if (maxY < 0) {
            minY = 0;
            maxY = -1;
        }
    }

    BoundingBox Chunk::bounds() const {
        const auto x0 = static_cast<float>(key.cx * cfg.chunkSize);
        const auto z0 = static_cast<float>(key.cz * cfg.chunkSize);
        if (emptyColumn()) {
            // Degenerate box: never passes the frustum test, which is what we want.
            return BoundingBox{{x0, 0.0f, z0}, {x0, 0.0f, z0}};
        }
        const auto side = static_cast<float>(cfg.chunkSize);
        return BoundingBox{{x0, static_cast<float>(minY), z0},
                           {x0 + side, static_cast<float>(maxY + 1), z0 + side}};
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
        const int pcx = floorDivInt(static_cast<int>(std::floor(playerPos.x)), cfg.chunkSize);
        const int pcz = floorDivInt(static_cast<int>(std::floor(playerPos.z)), cfg.chunkSize);

        // Read the mutable settings fresh every frame so the settings panel can
        // grow or shrink the streamed region while the game runs.
        const int viewR = std::max(0, settings.viewRadius);
        // Never drop a chunk we still want, but keep the unload ring tied to the
        // view radius so lowering it actually frees chunks.
        const int unloadR = std::clamp(settings.unloadRadius, viewR + 1, viewR + 4);

        // Generate missing chunks nearest-first, limited per frame.
        int genBudget = cfg.genPerFrame;
        for (int r = 0; r <= viewR && genBudget > 0; ++r) {
            for (int dz = -r; dz <= r && genBudget > 0; ++dz) {
                for (int dx = -r; dx <= r && genBudget > 0; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r)
                        continue; // ring only
                    const ChunkKey key{pcx + dx, pcz + dz};
                    if (chunks_.contains(key))
                        continue;
                    Chunk chunk;
                    chunk.key = key;
                    chunk.allocate();
                    generateChunk(chunk);
                    applyEditsTo(chunk);
                    chunk.recomputeExtent();
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
            if (std::max(std::abs(key.cx - pcx), std::abs(key.cz - pcz)) > unloadR) {
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

        // Light budget: run ahead of the mesher, nearest chunks first. The mesher
        // still forces a light pass on demand, so falling behind is never a
        // correctness problem - only a scheduling one.
        struct LightCandidate {
            Chunk* chunk;
            float dist;
        };
        std::vector<LightCandidate> unlit;
        for (auto& [key, chunk] : chunks_) {
            if (!chunk.lightDirty)
                continue;
            const auto dx = static_cast<float>(key.cx - pcx);
            const auto dz = static_cast<float>(key.cz - pcz);
            unlit.push_back({&chunk, dx * dx + dz * dz});
        }
        pendingLightCount_ = static_cast<int>(unlit.size());
        const int lightBudget = std::max(0, cfg.lightPerFrame);
        if (!unlit.empty() && lightBudget > 0) {
            const auto take = std::min(static_cast<std::size_t>(lightBudget), unlit.size());
            std::partial_sort(unlit.begin(), unlit.begin() + static_cast<std::ptrdiff_t>(take),
                              unlit.end(), [](const LightCandidate& a, const LightCandidate& b) {
                                  return a.dist < b.dist;
                              });
            for (std::size_t i = 0; i < take; ++i) {
                light::computeChunk(*this, *unlit[i].chunk);
                pendingLightCount_--;
            }
        }

        pendingMeshCount_ = 0;
        for (auto& [key, chunk] : chunks_) {
            if (chunk.meshDirty)
                pendingMeshCount_++;
        }
    }

    Chunk* World::chunkByKey(const ChunkKey key) {
        const auto it = chunks_.find(key);
        return it == chunks_.end() ? nullptr : &it->second;
    }

    const Chunk* World::chunkByKey(const ChunkKey key) const {
        const auto it = chunks_.find(key);
        return it == chunks_.end() ? nullptr : &it->second;
    }

    Chunk* World::chunkAt(const int x, const int z) { return chunkByKey(keyOf(x, z)); }

    const Chunk* World::chunkAt(const int x, const int z) const { return chunkByKey(keyOf(x, z)); }

    Block World::block(const int x, const int y, const int z) const {
        if (y < 0 || y >= cfg.worldHeight)
            return Block::Air;
        const Chunk* c = chunkAt(x, z);
        if (c == nullptr)
            return Block::Stone; // ungenerated area acts solid so nothing falls out of the world
        return c->at(modInt(x, cfg.chunkSize), y, modInt(z, cfg.chunkSize));
    }

    int World::skyLight(const int x, const int y, const int z) const {
        if (y >= cfg.worldHeight)
            return light::kMax;
        if (y < 0)
            return 0;
        const Chunk* c = chunkAt(x, z);
        return c == nullptr ? 0 : c->skyAt(modInt(x, cfg.chunkSize), y, modInt(z, cfg.chunkSize));
    }

    int World::blockLight(const int x, const int y, const int z) const {
        if (y < 0 || y >= cfg.worldHeight)
            return 0;
        const Chunk* c = chunkAt(x, z);
        return c == nullptr ? 0
                            : c->blockLightAt(modInt(x, cfg.chunkSize), y, modInt(z, cfg.chunkSize));
    }

    void World::setBlock(const int x, const int y, const int z, const Block b) {
        if (y < 1 || y >= cfg.worldHeight) // keep bedrock floor intact
            return;
        Chunk* chunk = chunkAt(x, z);
        if (chunk == nullptr)
            return;
        const int lx = modInt(x, cfg.chunkSize);
        const int lz = modInt(z, cfg.chunkSize);
        const Block before = chunk->at(lx, y, lz);

        chunk->set(lx, y, lz, b);
        edits_[chunk->key][packCoord(x, y, z)] = static_cast<std::uint8_t>(b);

        // Keep the culling extent honest; only a removal at the very top or bottom
        // of the span needs the full rescan.
        if (b != Block::Air) {
            chunk->expandExtent(y);
        } else if (y == chunk->maxY || y == chunk->minY) {
            chunk->recomputeExtent();
        }

        if (before != b) {
            light::updateBlock(*this, x, y, z, before, b);
        }

        // Dirty this chunk and any neighbor sharing the touched face column.
        markDirty(x, z);
        if (lx == 0) markDirty(x - 1, z);
        if (lx == cfg.chunkSize - 1) markDirty(x + 1, z);
        if (lz == 0) markDirty(x, z - 1);
        if (lz == cfg.chunkSize - 1) markDirty(x, z + 1);
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
        if (Chunk* c = chunkAt(x, z)) {
            c->meshDirty = true;
        }
    }

    std::size_t World::editCount() const {
        std::size_t total = 0;
        for (const auto& [key, bucket] : edits_) {
            total += bucket.size();
        }
        return total;
    }

    // --------------------------------------------------------- Generation

    void World::generateChunk(Chunk& chunk) const {
        worldgen::generateChunk(chunk, seed_);
    }

    void World::applyEditsTo(Chunk& chunk) const {
        const auto it = edits_.find(chunk.key);
        if (it == edits_.end())
            return; // O(edits in this chunk), not O(all edits)
        const int baseX = chunk.key.cx * cfg.chunkSize;
        const int baseZ = chunk.key.cz * cfg.chunkSize;
        for (const auto& [packed, value] : it->second) {
            int x = 0, y = 0, z = 0;
            unpackCoord(packed, x, y, z);
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

    // -------------------------------------------------------------- Culling

    bool chunkInFrustum(const Camera3D& cam, const float aspect, const BoundingBox& box) {
        // Match whatever clip planes rlgl is actually rendering with, so the test
        // never culls something the GPU would have kept.
        const double nearZ = rlGetCullDistanceNear();
        const double farZ = rlGetCullDistanceFar();
        const Matrix view = GetCameraMatrix(cam);
        Matrix proj;
        if (cam.projection == CAMERA_ORTHOGRAPHIC) {
            const double top = static_cast<double>(cam.fovy) * 0.5;
            const double right = top * static_cast<double>(aspect);
            proj = MatrixOrtho(-right, right, -top, top, nearZ, farZ);
        } else {
            proj = MatrixPerspective(static_cast<double>(cam.fovy) * DEG2RAD,
                                     static_cast<double>(aspect), nearZ, farZ);
        }
        // raylib's MatrixMultiply(a, b) evaluates to b * a in column-vector terms,
        // so this is the clip matrix P * V that the shader's `mvp` uses.
        const Matrix clip = MatrixMultiply(view, proj);

        // Gribb-Hartmann: rows of the clip matrix are (m0,m4,m8,m12), (m1,m5,m9,m13), ...
        const float rows[4][4] = {
            {clip.m0, clip.m4, clip.m8, clip.m12},
            {clip.m1, clip.m5, clip.m9, clip.m13},
            {clip.m2, clip.m6, clip.m10, clip.m14},
            {clip.m3, clip.m7, clip.m11, clip.m15},
        };
        float planes[6][4];
        for (int i = 0; i < 3; ++i) {
            for (int c = 0; c < 4; ++c) {
                planes[i * 2][c] = rows[3][c] + rows[i][c];     // left / bottom / near
                planes[i * 2 + 1][c] = rows[3][c] - rows[i][c]; // right / top / far
            }
        }

        // An AABB is outside a plane only if its most-positive corner is behind it.
        for (const auto& p : planes) {
            const float px = p[0] >= 0.0f ? box.max.x : box.min.x;
            const float py = p[1] >= 0.0f ? box.max.y : box.min.y;
            const float pz = p[2] >= 0.0f ? box.max.z : box.min.z;
            if (p[0] * px + p[1] * py + p[2] * pz + p[3] < 0.0f)
                return false;
        }
        return true;
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
        const auto count = static_cast<std::uint32_t>(editCount());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [key, bucket] : edits_) {
            for (const auto& [packed, value] : bucket) {
                out.write(reinterpret_cast<const char*>(&packed), sizeof(packed));
                out.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
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
            std::uint64_t packed = 0;
            std::uint8_t value = 0;
            in.read(reinterpret_cast<char*>(&packed), sizeof(packed));
            in.read(reinterpret_cast<char*>(&value), sizeof(value));
            int x = 0, y = 0, z = 0;
            unpackCoord(packed, x, y, z);
            edits_[keyOf(x, z)][packed] = value;
        }
        TraceLog(LOG_INFO, "VOXHAVEN: loaded %zu edits (seed %u)", editCount(), seed_);
    }

} // namespace vox
