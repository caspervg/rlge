#include "vx_mesher.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "raylib.h"
#include "rlgl.h"

#include "vx_light.hpp"

namespace vox {

    namespace {
        struct FaceDef {
            int n[3];        // normal
            int c[4][3];     // corner offsets (CCW from outside)
            float u[4];      // tile-local UVs per corner
            float v[4];
            float shade;     // directional light bake
            int uAxis;       // world axis the U coordinate runs along
            int vAxis;       // world axis the V coordinate runs along
        };

        // Winding chosen so raylib's backface culling keeps outside faces.
        // uAxis/vAxis record which world axis each texture coordinate follows, so a
        // merged quad knows how many tiles it has to repeat in each direction.
        constexpr FaceDef kFaces[6] = {
            // +Y top
            {{0, 1, 0},
             {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},
             {0, 0, 1, 1}, {0, 1, 1, 0}, 1.0f, 0, 2},
            // -Y bottom
            {{0, -1, 0},
             {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
             {0, 1, 1, 0}, {0, 0, 1, 1}, 0.5f, 0, 2},
            // +X
            {{1, 0, 0},
             {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
             {0, 0, 1, 1}, {1, 0, 0, 1}, 0.64f, 2, 1},
            // -X
            {{-1, 0, 0},
             {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
             {0, 1, 1, 0}, {1, 1, 0, 0}, 0.64f, 2, 1},
            // +Z
            {{0, 0, 1},
             {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
             {0, 1, 1, 0}, {1, 1, 0, 0}, 0.8f, 0, 1},
            // -Z
            {{0, 0, -1},
             {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
             {0, 0, 1, 1}, {1, 0, 0, 1}, 0.8f, 0, 1},
        };

        constexpr float kAoLevels[4] = {0.45f, 0.65f, 0.82f, 1.0f};

        // ------------------------------------------------------------ snapshot

        // Padded copy of the chunk plus its eight neighbours, indexed in the
        // chunk's local frame over [-1, chunkSize]. Building this once per chunk
        // replaces roughly 1.9M hash-map lookups with plain array indexing: every
        // neighbour and AO probe the mesher needs falls inside the pad.
        struct Snapshot {
            int sx = 0; // chunkSize + 2
            int sy = 0; // worldHeight
            std::vector<std::uint8_t> blocks;
            std::vector<std::uint8_t> light;

            [[nodiscard]] int idx(const int lx, const int y, const int lz) const {
                return (y * sx + (lz + 1)) * sx + (lx + 1);
            }
            [[nodiscard]] Block block(const int lx, const int y, const int lz) const {
                if (y < 0 || y >= sy)
                    return Block::Air;
                return static_cast<Block>(blocks[static_cast<std::size_t>(idx(lx, y, lz))]);
            }
            [[nodiscard]] bool opaque(const int lx, const int y, const int lz) const {
                return blockInfo(block(lx, y, lz)).opaque;
            }
            [[nodiscard]] int sky(const int lx, const int y, const int lz) const {
                if (y >= sy)
                    return light::kMax; // open sky above the world
                if (y < 0)
                    return 0;
                return light[static_cast<std::size_t>(idx(lx, y, lz))] >> 4;
            }
            [[nodiscard]] int blockLight(const int lx, const int y, const int lz) const {
                if (y < 0 || y >= sy)
                    return 0;
                return light[static_cast<std::size_t>(idx(lx, y, lz))] & 0x0F;
            }
        };

        void fillSnapshot(World& world, const Chunk& chunk, Snapshot& s) {
            const int cs = cfg.chunkSize;
            const int wh = cfg.worldHeight;
            s.sx = cs + 2;
            s.sy = wh;
            const auto count = static_cast<std::size_t>(s.sx) * static_cast<std::size_t>(s.sx) *
                               static_cast<std::size_t>(wh);
            // Ungenerated space reads as stone, exactly like World::block(), so
            // border faces stay hidden until the neighbour actually arrives.
            s.blocks.assign(count, static_cast<std::uint8_t>(Block::Stone));
            s.light.assign(count, 0);

            for (int ncz = -1; ncz <= 1; ++ncz) {
                for (int ncx = -1; ncx <= 1; ++ncx) {
                    const Chunk* src =
                        world.chunkByKey({chunk.key.cx + ncx, chunk.key.cz + ncz});
                    if (src == nullptr)
                        continue;
                    const int x0 = std::max(-1, ncx * cs);
                    const int x1 = std::min(cs, ncx * cs + cs - 1);
                    const int z0 = std::max(-1, ncz * cs);
                    const int z1 = std::min(cs, ncz * cs + cs - 1);
                    if (x0 > x1 || z0 > z1)
                        continue;
                    for (int y = 0; y < wh; ++y) {
                        for (int lz = z0; lz <= z1; ++lz) {
                            const int srcRow = Chunk::index(0, y, lz - ncz * cs);
                            auto dst = static_cast<std::size_t>(s.idx(x0, y, lz));
                            for (int lx = x0; lx <= x1; ++lx, ++dst) {
                                const auto si =
                                    static_cast<std::size_t>(srcRow + (lx - ncx * cs));
                                s.blocks[dst] = src->blocks[si];
                                s.light[dst] = src->light[si];
                            }
                        }
                    }
                }
            }
        }

        // --------------------------------------------------------------- quads

        // One candidate face in a greedy slice mask.
        struct Quad {
            bool present = false;
            bool water = false;
            std::uint16_t tile = 0;
            std::uint8_t alpha = 255;
            std::uint8_t ao[4]{};  // kAoLevels index
            std::uint8_t sky[4]{};
            std::uint8_t blk[4]{};

            [[nodiscard]] bool matches(const Quad& o) const {
                return o.present && water == o.water && tile == o.tile && alpha == o.alpha &&
                       std::equal(ao, ao + 4, o.ao) && std::equal(sky, sky + 4, o.sky) &&
                       std::equal(blk, blk + 4, o.blk);
            }

            // Do two corners of this face carry identical shading?
            [[nodiscard]] bool cornersAgree(const int i, const int j) const {
                return ao[i] == ao[j] && sky[i] == sky[j] && blk[i] == blk[j];
            }
        };

        // Visibility + per-vertex AO and smooth light for one block face.
        bool faceQuad(const Snapshot& s, const int f, const int lx, const int y, const int lz,
                      Quad& out) {
            const FaceDef& face = kFaces[f];
            const Block b = s.block(lx, y, lz);
            if (b == Block::Air)
                return false;

            const int nx = lx + face.n[0];
            const int ny = y + face.n[1];
            const int nz = lz + face.n[2];
            const Block neighbor = s.block(nx, ny, nz);
            const BlockInfo& info = blockInfo(b);

            out = Quad{};
            const bool isWater = b == Block::Water;
            if (isWater) {
                // Water renders only against air (and the sky above).
                if (neighbor != Block::Air)
                    return false;
                out.water = true;
                out.alpha = 170;
                out.tile = static_cast<std::uint16_t>(info.tileTop);
            } else {
                if (blockInfo(neighbor).opaque)
                    return false;
                if (info.cutout && neighbor == b)
                    return false; // no internal faces between identical cutout blocks
                out.alpha = 255;
                out.tile = static_cast<std::uint16_t>(face.n[1] > 0   ? info.tileTop
                                                      : face.n[1] < 0 ? info.tileBottom
                                                                      : info.tileSide);
            }
            out.present = true;

            const int axis = face.n[0] != 0 ? 0 : (face.n[1] != 0 ? 1 : 2);
            const int t1 = axis == 0 ? 1 : 0;
            const int t2 = axis == 2 ? 1 : 2;
            for (int i = 0; i < 4; ++i) {
                const int* corner = face.c[i];
                int d1[3] = {0, 0, 0};
                int d2[3] = {0, 0, 0};
                d1[t1] = corner[t1] == 1 ? 1 : -1;
                d2[t2] = corner[t2] == 1 ? 1 : -1;
                const int ax = nx + d1[0], ay = ny + d1[1], az = nz + d1[2];
                const int bx = nx + d2[0], by = ny + d2[1], bz = nz + d2[2];
                const int cx = nx + d1[0] + d2[0], cy = ny + d1[1] + d2[1], cz = nz + d1[2] + d2[2];
                const bool o1 = s.opaque(ax, ay, az);
                const bool o2 = s.opaque(bx, by, bz);
                const bool oc = s.opaque(cx, cy, cz);

                // Water keeps the flat, un-occluded look it had before.
                out.ao[i] = isWater ? 3
                                    : static_cast<std::uint8_t>(
                                          (o1 && o2) ? 0
                                                     : 3 - (static_cast<int>(o1) +
                                                            static_cast<int>(o2) +
                                                            static_cast<int>(oc)));

                // Smooth lighting: average the light of the voxels touching this
                // corner from outside, skipping the opaque ones so AO is the only
                // thing that darkens corners.
                int skySum = s.sky(nx, ny, nz);
                int blkSum = s.blockLight(nx, ny, nz);
                int n = 1;
                if (!o1) { skySum += s.sky(ax, ay, az); blkSum += s.blockLight(ax, ay, az); ++n; }
                if (!o2) { skySum += s.sky(bx, by, bz); blkSum += s.blockLight(bx, by, bz); ++n; }
                if (!oc) { skySum += s.sky(cx, cy, cz); blkSum += s.blockLight(cx, cy, cz); ++n; }
                out.sky[i] = static_cast<std::uint8_t>((skySum + n / 2) / n);
                out.blk[i] = static_cast<std::uint8_t>((blkSum + n / 2) / n);
            }

            return true;
        }

        // -------------------------------------------------------- mesh buffers

        struct MeshBuffer {
            std::vector<float> vertices;
            std::vector<float> texcoords;
            std::vector<float> tileOrigins; // uploaded as texcoords2
            std::vector<float> normals;
            std::vector<unsigned char> colors;
            std::vector<unsigned short> indices;

            [[nodiscard]] int vertexCount() const { return static_cast<int>(vertices.size() / 3); }

            [[nodiscard]] bool full() const { return vertexCount() > 65000; }

            void clear() {
                vertices.clear();
                texcoords.clear();
                tileOrigins.clear();
                normals.clear();
                colors.clear();
                indices.clear();
            }

            // `ext` is the quad's size along each world axis (1 on the face's own
            // axis, the merged run lengths on the other two).
            void pushQuad(const FaceDef& face, const float bx, const float by, const float bz,
                          const int ext[3], const Rectangle uv, const Quad& quad,
                          const float yScaleTop) {
                const auto base = static_cast<unsigned short>(vertexCount());
                const auto repU = static_cast<float>(ext[face.uAxis]);
                const auto repV = static_cast<float>(ext[face.vAxis]);
                float ao[4];
                for (int i = 0; i < 4; ++i) {
                    ao[i] = kAoLevels[quad.ao[i]];
                }
                for (int i = 0; i < 4; ++i) {
                    vertices.push_back(bx + (face.c[i][0] != 0 ? static_cast<float>(ext[0]) : 0.0f));
                    vertices.push_back(
                        by + (face.c[i][1] != 0 ? static_cast<float>(ext[1]) * yScaleTop : 0.0f));
                    vertices.push_back(bz + (face.c[i][2] != 0 ? static_cast<float>(ext[2]) : 0.0f));
                    texcoords.push_back(uv.x + face.u[i] * uv.width * repU);
                    texcoords.push_back(uv.y + face.v[i] * uv.height * repV);
                    tileOrigins.push_back(uv.x);
                    tileOrigins.push_back(uv.y);
                    normals.push_back(static_cast<float>(face.n[0]));
                    normals.push_back(static_cast<float>(face.n[1]));
                    normals.push_back(static_cast<float>(face.n[2]));
                    const auto ch = [](const float f) {
                        return static_cast<unsigned char>(std::clamp(f * 255.0f, 0.0f, 255.0f));
                    };
                    colors.push_back(ch(face.shade * ao[i]));                                  // R
                    colors.push_back(ch(static_cast<float>(quad.sky[i]) / light::kMax));       // G
                    colors.push_back(ch(static_cast<float>(quad.blk[i]) / light::kMax));       // B
                    colors.push_back(quad.alpha);
                }
                // Flip the quad split when AO is anisotropic to avoid seams.
                if (ao[0] + ao[2] >= ao[1] + ao[3]) {
                    const unsigned short tri[6] = {base, static_cast<unsigned short>(base + 1),
                                                   static_cast<unsigned short>(base + 2), base,
                                                   static_cast<unsigned short>(base + 2),
                                                   static_cast<unsigned short>(base + 3)};
                    indices.insert(indices.end(), tri, tri + 6);
                } else {
                    const unsigned short tri[6] = {static_cast<unsigned short>(base + 1),
                                                   static_cast<unsigned short>(base + 2),
                                                   static_cast<unsigned short>(base + 3),
                                                   static_cast<unsigned short>(base + 1),
                                                   static_cast<unsigned short>(base + 3), base};
                    indices.insert(indices.end(), tri, tri + 6);
                }
            }

            [[nodiscard]] Mesh upload() const {
                Mesh mesh{};
                const int vc = vertexCount();
                if (vc == 0)
                    return mesh;
                mesh.vertexCount = vc;
                mesh.triangleCount = static_cast<int>(indices.size() / 3);
                mesh.vertices = static_cast<float*>(RL_MALLOC(vertices.size() * sizeof(float)));
                std::memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
                mesh.texcoords = static_cast<float*>(RL_MALLOC(texcoords.size() * sizeof(float)));
                std::memcpy(mesh.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
                mesh.texcoords2 =
                    static_cast<float*>(RL_MALLOC(tileOrigins.size() * sizeof(float)));
                std::memcpy(mesh.texcoords2, tileOrigins.data(),
                            tileOrigins.size() * sizeof(float));
                mesh.normals = static_cast<float*>(RL_MALLOC(normals.size() * sizeof(float)));
                std::memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
                mesh.colors = static_cast<unsigned char*>(RL_MALLOC(colors.size()));
                std::memcpy(mesh.colors, colors.data(), colors.size());
                mesh.indices = static_cast<unsigned short*>(
                    RL_MALLOC(indices.size() * sizeof(unsigned short)));
                std::memcpy(mesh.indices, indices.data(), indices.size() * sizeof(unsigned short));
                UploadMesh(&mesh, false);
                return mesh;
            }
        };
    } // namespace

    int Mesher::remeshDirty(World& world, const Vector3 playerPos, const int budget) {
        struct Candidate {
            Chunk* chunk;
            float dist;
        };
        std::vector<Candidate> dirty;
        const float px = playerPos.x;
        const float pz = playerPos.z;
        for (auto& [key, chunk] : world.chunks()) {
            if (!chunk.meshDirty)
                continue;
            const float cx = (static_cast<float>(key.cx) + 0.5f) * static_cast<float>(cfg.chunkSize);
            const float cz = (static_cast<float>(key.cz) + 0.5f) * static_cast<float>(cfg.chunkSize);
            const float dist = (cx - px) * (cx - px) + (cz - pz) * (cz - pz);
            dirty.push_back({&chunk, dist});
        }
        std::sort(dirty.begin(), dirty.end(),
                  [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });

        int done = 0;
        for (const auto& [chunk, dist] : dirty) {
            if (done >= budget)
                break;
            buildChunk(world, *chunk);
            done++;
        }
        return done;
    }

    void Mesher::buildChunk(World& world, Chunk& chunk) {
        // Border faces sample light out of the neighbours, so they have to be lit
        // before we bake vertex colors. Normally World::update has already done
        // this; the call is a no-op then.
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                light::ensureChunkAt(world, {chunk.key.cx + dx, chunk.key.cz + dz});
            }
        }

        // Reused across chunks: the snapshot is ~62 KB and the mask up to 96x96.
        static thread_local Snapshot snap;
        static thread_local std::vector<Quad> mask;
        static thread_local MeshBuffer opaque;
        static thread_local MeshBuffer water;
        fillSnapshot(world, chunk, snap);
        opaque.clear();
        water.clear();

        const int cs = cfg.chunkSize;
        const int wh = cfg.worldHeight;
        const int dim[3] = {cs, wh, cs};
        const int baseX = chunk.key.cx * cs;
        const int baseZ = chunk.key.cz * cs;

        for (int f = 0; f < 6; ++f) {
            const FaceDef& face = kFaces[f];
            const int axis = face.n[0] != 0 ? 0 : (face.n[1] != 0 ? 1 : 2);
            const int uax = axis == 0 ? 1 : 0; // the two in-plane axes, ascending
            const int vax = axis == 2 ? 1 : 2;
            const int dimU = dim[uax];
            const int dimV = dim[vax];
            const bool isTop = face.n[1] > 0;
            mask.resize(static_cast<std::size_t>(dimU) * static_cast<std::size_t>(dimV));

            // Which face corner sits at each (u-side, v-side) of the quad. Used to
            // decide along which axis a merge would leave the shading untouched.
            int corner[2][2] = {};
            for (int i = 0; i < 4; ++i) {
                corner[face.c[i][uax]][face.c[i][vax]] = i;
            }

            for (int slice = 0; slice < dim[axis]; ++slice) {
                for (int q = 0; q < dimV; ++q) {
                    for (int p = 0; p < dimU; ++p) {
                        int c[3];
                        c[axis] = slice;
                        c[uax] = p;
                        c[vax] = q;
                        Quad& cell = mask[static_cast<std::size_t>(q) * dimU + p];
                        if (!faceQuad(snap, f, c[0], c[1], c[2], cell))
                            cell.present = false;
                    }
                }

                for (int q = 0; q < dimV; ++q) {
                    for (int p = 0; p < dimU;) {
                        const Quad cell = mask[static_cast<std::size_t>(q) * dimU + p];
                        if (!cell.present) {
                            ++p;
                            continue;
                        }
                        // Stretching a quad resamples its corner interpolation, so a
                        // merge is only lossless along an axis the shading does not
                        // vary over. Growing height with width > 1 implies both
                        // hold, i.e. the face is flat-shaded.
                        int w = 1;
                        int h = 1;
                        if (greedy) {
                            const bool constU = cell.cornersAgree(corner[0][0], corner[1][0]) &&
                                                cell.cornersAgree(corner[0][1], corner[1][1]);
                            const bool constV = cell.cornersAgree(corner[0][0], corner[0][1]) &&
                                                cell.cornersAgree(corner[1][0], corner[1][1]);
                            if (constU) {
                                while (p + w < dimU &&
                                       cell.matches(mask[static_cast<std::size_t>(q) * dimU + p + w]))
                                    ++w;
                            }
                            if (constV) {
                                bool grow = true;
                                while (q + h < dimV && grow) {
                                    for (int k = 0; k < w; ++k) {
                                        if (!cell.matches(
                                                mask[static_cast<std::size_t>(q + h) * dimU + p + k])) {
                                            grow = false;
                                            break;
                                        }
                                    }
                                    if (grow)
                                        ++h;
                                }
                            }
                        }

                        MeshBuffer& buf = cell.water ? water : opaque;
                        if (!buf.full()) {
                            int base[3];
                            base[axis] = slice;
                            base[uax] = p;
                            base[vax] = q;
                            int ext[3] = {1, 1, 1};
                            ext[uax] = w;
                            ext[vax] = h;
                            buf.pushQuad(face, static_cast<float>(baseX + base[0]),
                                         static_cast<float>(base[1]),
                                         static_cast<float>(baseZ + base[2]), ext, tileUV(cell.tile),
                                         cell, cell.water && isTop ? 0.86f : 1.0f);
                        }

                        for (int dq = 0; dq < h; ++dq) {
                            for (int dp = 0; dp < w; ++dp) {
                                mask[static_cast<std::size_t>(q + dq) * dimU + p + dp].present =
                                    false;
                            }
                        }
                        p += w;
                    }
                }
            }
        }

        // Swap in the fresh meshes.
        if (chunk.hasMesh) {
            UnloadMesh(chunk.opaqueMesh);
            UnloadMesh(chunk.waterMesh);
            chunk.opaqueMesh = Mesh{};
            chunk.waterMesh = Mesh{};
            chunk.hasMesh = false;
        }
        chunk.opaqueMesh = opaque.upload();
        chunk.waterMesh = water.upload();
        chunk.hasMesh = true;
        chunk.meshDirty = false;
    }

} // namespace vox
