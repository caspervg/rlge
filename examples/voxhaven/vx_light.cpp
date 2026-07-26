#include "vx_light.hpp"

#include <algorithm>
#include <deque>

namespace vox::light {

    namespace {

        struct Node {
            int x, y, z;
            int level;
        };

        enum class Channel : std::uint8_t { Sky, Block };

        constexpr int kDir[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        };

        // Memoised world -> chunk resolution. A flood walks a compact region, so
        // remembering the last chunk turns nearly every access into an array index
        // instead of a hash lookup. Pointers stay valid because floods never
        // insert into or erase from the chunk map.
        class Access {
        public:
            explicit Access(World& world) :
                world_(&world) {}

            Chunk* chunkFor(const int x, const int z) {
                const ChunkKey key{floorDivInt(x, cfg.chunkSize), floorDivInt(z, cfg.chunkSize)};
                if (valid_ && key == key_)
                    return chunk_;
                auto& map = world_->chunks();
                const auto it = map.find(key);
                chunk_ = it == map.end() ? nullptr : &it->second;
                key_ = key;
                valid_ = true;
                return chunk_;
            }

            [[nodiscard]] int get(const Channel ch, const int x, const int y, const int z) {
                if (y < 0 || y >= cfg.worldHeight)
                    return 0;
                const Chunk* c = chunkFor(x, z);
                if (c == nullptr)
                    return 0;
                const int lx = modInt(x, cfg.chunkSize);
                const int lz = modInt(z, cfg.chunkSize);
                return ch == Channel::Sky ? c->skyAt(lx, y, lz) : c->blockLightAt(lx, y, lz);
            }

        private:
            World* world_;
            Chunk* chunk_ = nullptr;
            ChunkKey key_{};
            bool valid_ = false;
        };

        void setLight(Chunk& c, const Channel ch, const int lx, const int y, const int lz,
                      const int v) {
            if (ch == Channel::Sky)
                c.setSkyAt(lx, y, lz, v);
            else
                c.setBlockLightAt(lx, y, lz, v);
            c.meshDirty = true; // any light change invalidates the baked vertex colors
        }

        int getLight(const Chunk& c, const Channel ch, const int lx, const int y, const int lz) {
            return ch == Channel::Sky ? c.skyAt(lx, y, lz) : c.blockLightAt(lx, y, lz);
        }

        // Standard increasing flood: a neighbour is raised to level-1-opacity, with
        // the one exception that full-strength skylight falls straight down through
        // clear blocks without losing a level.
        void flood(Access& acc, std::deque<Node>& queue, const Channel ch) {
            const int wh = cfg.worldHeight;
            const int cs = cfg.chunkSize;
            while (!queue.empty()) {
                const Node n = queue.front();
                queue.pop_front();
                if (n.level <= 1)
                    continue;
                if (acc.get(ch, n.x, n.y, n.z) != n.level)
                    continue; // superseded by a brighter visit
                for (const auto& d : kDir) {
                    const int nx = n.x + d[0];
                    const int ny = n.y + d[1];
                    const int nz = n.z + d[2];
                    if (ny < 0 || ny >= wh)
                        continue;
                    Chunk* c = acc.chunkFor(nx, nz);
                    if (c == nullptr)
                        continue; // never light into ungenerated space
                    const int lx = modInt(nx, cs);
                    const int lz = modInt(nz, cs);
                    const int op = blockInfo(c->at(lx, ny, lz)).lightOpacity;
                    if (op >= kMax)
                        continue;
                    const bool straightDownSun =
                        ch == Channel::Sky && d[1] < 0 && n.level == kMax && op == 0;
                    const int next = straightDownSun ? kMax : n.level - 1 - op;
                    if (next <= 0)
                        continue;
                    if (getLight(*c, ch, lx, ny, lz) >= next)
                        continue;
                    setLight(*c, ch, lx, ny, lz, next);
                    queue.push_back({nx, ny, nz, next});
                }
            }
        }

        // Classic removal pass: zero every neighbour whose level could only have
        // come from us, and collect the brighter ones as refill seeds.
        void unflood(Access& acc, std::deque<Node>& queue, std::deque<Node>& refill,
                     const Channel ch) {
            const int wh = cfg.worldHeight;
            const int cs = cfg.chunkSize;
            while (!queue.empty()) {
                const Node n = queue.front();
                queue.pop_front();
                for (const auto& d : kDir) {
                    const int nx = n.x + d[0];
                    const int ny = n.y + d[1];
                    const int nz = n.z + d[2];
                    if (ny < 0 || ny >= wh)
                        continue;
                    Chunk* c = acc.chunkFor(nx, nz);
                    if (c == nullptr)
                        continue;
                    const int lx = modInt(nx, cs);
                    const int lz = modInt(nz, cs);
                    const int cur = getLight(*c, ch, lx, ny, lz);
                    if (cur == 0)
                        continue;
                    // Equal-strength skylight directly below a removed full-strength
                    // voxel can only have been the same sun column, so it dies too.
                    const bool derived =
                        cur < n.level ||
                        (ch == Channel::Sky && d[1] < 0 && n.level == kMax && cur == kMax);
                    if (derived) {
                        setLight(*c, ch, lx, ny, lz, 0);
                        queue.push_back({nx, ny, nz, cur});
                    } else {
                        refill.push_back({nx, ny, nz, cur});
                    }
                }
            }
        }

        // Push every loaded neighbour of a voxel as a refill seed so light can flow
        // back into (and through) a voxel that just changed.
        void seedNeighbours(Access& acc, std::deque<Node>& refill, const Channel ch, const int x,
                            const int y, const int z) {
            for (const auto& d : kDir) {
                const int nx = x + d[0];
                const int ny = y + d[1];
                const int nz = z + d[2];
                if (ny < 0 || ny >= cfg.worldHeight)
                    continue;
                const int v = acc.get(ch, nx, ny, nz);
                if (v > 1)
                    refill.push_back({nx, ny, nz, v});
            }
        }

        // Seed the flood from the four horizontal neighbours' border columns so a
        // freshly lit chunk picks up whatever its already-lit neighbours have.
        void seedFromNeighbours(World& world, const Chunk& chunk, std::deque<Node>& queue,
                                const Channel ch) {
            const int cs = cfg.chunkSize;
            const int wh = cfg.worldHeight;
            const int baseX = chunk.key.cx * cs;
            const int baseZ = chunk.key.cz * cs;
            constexpr int sides[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
            for (const auto& s : sides) {
                const Chunk* n = world.chunkByKey({chunk.key.cx + s[0], chunk.key.cz + s[1]});
                if (n == nullptr || n->lightDirty)
                    continue;
                for (int y = 0; y < wh; ++y) {
                    for (int t = 0; t < cs; ++t) {
                        const int wx = s[0] != 0 ? baseX + (s[0] < 0 ? -1 : cs) : baseX + t;
                        const int wz = s[1] != 0 ? baseZ + (s[1] < 0 ? -1 : cs) : baseZ + t;
                        const int lx = modInt(wx, cs);
                        const int lz = modInt(wz, cs);
                        const int v = getLight(*n, ch, lx, y, lz);
                        if (v > 1)
                            queue.push_back({wx, y, wz, v});
                    }
                }
            }
        }

    } // namespace

    void computeChunk(World& world, Chunk& chunk) {
        const int cs = cfg.chunkSize;
        const int wh = cfg.worldHeight;
        const int baseX = chunk.key.cx * cs;
        const int baseZ = chunk.key.cz * cs;

        std::fill(chunk.light.begin(), chunk.light.end(), std::uint8_t{0});
        chunk.lightDirty = false;
        chunk.meshDirty = true;

        Access acc(world);
        std::deque<Node> queue;

        // --- skylight ------------------------------------------------------
        // Vertical carry first: 15 straight down, losing only the opacity of each
        // block it enters (so clear blocks cost nothing).
        for (int lz = 0; lz < cs; ++lz) {
            for (int lx = 0; lx < cs; ++lx) {
                int level = kMax;
                for (int y = wh - 1; y >= 0; --y) {
                    level = std::max(0, level - blockInfo(chunk.at(lx, y, lz)).lightOpacity);
                    if (level == 0)
                        break;
                    chunk.setSkyAt(lx, y, lz, level);
                }
            }
        }
        // Only voxels that border something darker (or another chunk) can hand
        // light on, so seeding just those keeps the flood queue an order of
        // magnitude smaller than seeding the whole column volume.
        for (int y = 0; y < wh; ++y) {
            for (int lz = 0; lz < cs; ++lz) {
                for (int lx = 0; lx < cs; ++lx) {
                    const int level = chunk.skyAt(lx, y, lz);
                    if (level <= 1)
                        continue;
                    bool seed = lx == 0 || lz == 0 || lx == cs - 1 || lz == cs - 1;
                    if (!seed) {
                        seed = chunk.skyAt(lx - 1, y, lz) < level ||
                               chunk.skyAt(lx + 1, y, lz) < level ||
                               chunk.skyAt(lx, y, lz - 1) < level ||
                               chunk.skyAt(lx, y, lz + 1) < level;
                    }
                    if (seed)
                        queue.push_back({baseX + lx, y, baseZ + lz, level});
                }
            }
        }
        seedFromNeighbours(world, chunk, queue, Channel::Sky);
        flood(acc, queue, Channel::Sky);

        // --- block light ---------------------------------------------------
        queue.clear();
        for (int y = 0; y < wh; ++y) {
            for (int lz = 0; lz < cs; ++lz) {
                for (int lx = 0; lx < cs; ++lx) {
                    const int emit = blockInfo(chunk.at(lx, y, lz)).lightEmission;
                    if (emit <= 0)
                        continue;
                    chunk.setBlockLightAt(lx, y, lz, emit);
                    queue.push_back({baseX + lx, y, baseZ + lz, emit});
                }
            }
        }
        seedFromNeighbours(world, chunk, queue, Channel::Block);
        flood(acc, queue, Channel::Block);
    }

    void ensureChunk(World& world, Chunk& chunk) {
        if (chunk.lightDirty)
            computeChunk(world, chunk);
    }

    void ensureChunkAt(World& world, const ChunkKey key) {
        if (Chunk* c = world.chunkByKey(key))
            ensureChunk(world, *c);
    }

    void updateBlock(World& world, const int x, const int y, const int z, const Block before,
                     const Block after) {
        const BlockInfo& bi = blockInfo(before);
        const BlockInfo& ai = blockInfo(after);
        if (bi.lightOpacity == ai.lightOpacity && bi.lightEmission == ai.lightEmission)
            return; // the edit cannot have changed any light value

        Access acc(world);
        Chunk* home = acc.chunkFor(x, z);
        if (home == nullptr || home->lightDirty)
            return; // a full recompute for this chunk is already queued

        const int cs = cfg.chunkSize;
        const int wh = cfg.worldHeight;
        const int hx = modInt(x, cs);
        const int hz = modInt(z, cs);

        // --- block light ---------------------------------------------------
        {
            std::deque<Node> removal;
            std::deque<Node> refill;
            if (const int old = home->blockLightAt(hx, y, hz); old > 0) {
                setLight(*home, Channel::Block, hx, y, hz, 0);
                removal.push_back({x, y, z, old});
                unflood(acc, removal, refill, Channel::Block);
            }
            if (ai.lightEmission > 0) {
                setLight(*home, Channel::Block, hx, y, hz, ai.lightEmission);
                refill.push_back({x, y, z, ai.lightEmission});
            }
            seedNeighbours(acc, refill, Channel::Block, x, y, z);
            flood(acc, refill, Channel::Block);
        }

        // --- skylight ------------------------------------------------------
        if (bi.lightOpacity != ai.lightOpacity) {
            std::deque<Node> removal;
            std::deque<Node> refill;
            if (const int old = home->skyAt(hx, y, hz); old > 0) {
                setLight(*home, Channel::Sky, hx, y, hz, 0);
                removal.push_back({x, y, z, old});
                unflood(acc, removal, refill, Channel::Sky);
            }
            // Re-lay the whole sun column with exactly the rule computeChunk uses,
            // so an edit can never disagree with a later full recompute.
            int level = kMax;
            for (int yy = wh - 1; yy >= 0 && level > 0; --yy) {
                level = std::max(0, level - blockInfo(home->at(hx, yy, hz)).lightOpacity);
                if (level == 0)
                    break;
                if (level > home->skyAt(hx, yy, hz)) {
                    setLight(*home, Channel::Sky, hx, yy, hz, level);
                    refill.push_back({x, yy, z, level});
                }
            }
            seedNeighbours(acc, refill, Channel::Sky, x, y, z);
            flood(acc, refill, Channel::Sky);
        }
    }

} // namespace vox::light
