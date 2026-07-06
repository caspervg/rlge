#include "vx_mesher.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "raylib.h"
#include "rlgl.h"

namespace vox {

    namespace {
        struct FaceDef {
            int n[3];        // normal
            int c[4][3];     // corner offsets (CCW from outside)
            float u[4];      // tile-local UVs per corner
            float v[4];
            float shade;     // directional light bake
        };

        // Winding chosen so raylib's backface culling keeps outside faces.
        constexpr FaceDef kFaces[6] = {
            // +Y top
            {{0, 1, 0},
             {{0, 1, 0}, {0, 1, 1}, {1, 1, 1}, {1, 1, 0}},
             {0, 0, 1, 1}, {0, 1, 1, 0}, 1.0f},
            // -Y bottom
            {{0, -1, 0},
             {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
             {0, 1, 1, 0}, {0, 0, 1, 1}, 0.5f},
            // +X
            {{1, 0, 0},
             {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
             {0, 0, 1, 1}, {1, 0, 0, 1}, 0.64f},
            // -X
            {{-1, 0, 0},
             {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
             {0, 1, 1, 0}, {1, 1, 0, 0}, 0.64f},
            // +Z
            {{0, 0, 1},
             {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
             {0, 1, 1, 0}, {1, 1, 0, 0}, 0.8f},
            // -Z
            {{0, 0, -1},
             {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
             {0, 0, 1, 1}, {1, 0, 0, 1}, 0.8f},
        };

        constexpr float kAoLevels[4] = {0.45f, 0.65f, 0.82f, 1.0f};

        struct MeshBuffer {
            std::vector<float> vertices;
            std::vector<float> texcoords;
            std::vector<float> normals;
            std::vector<unsigned char> colors;
            std::vector<unsigned short> indices;

            [[nodiscard]] int vertexCount() const { return static_cast<int>(vertices.size() / 3); }

            bool full() const { return vertexCount() > 65000; }

            void pushFace(const FaceDef& face, const float bx, const float by, const float bz,
                          const Rectangle uv, const float ao[4], const unsigned char alpha,
                          const float yScaleTop) {
                const auto base = static_cast<unsigned short>(vertexCount());
                for (int i = 0; i < 4; ++i) {
                    float cy = static_cast<float>(face.c[i][1]);
                    if (cy > 0.5f)
                        cy = yScaleTop; // lowered water surface
                    vertices.push_back(bx + static_cast<float>(face.c[i][0]));
                    vertices.push_back(by + cy);
                    vertices.push_back(bz + static_cast<float>(face.c[i][2]));
                    texcoords.push_back(uv.x + face.u[i] * uv.width);
                    texcoords.push_back(uv.y + face.v[i] * uv.height);
                    normals.push_back(static_cast<float>(face.n[0]));
                    normals.push_back(static_cast<float>(face.n[1]));
                    normals.push_back(static_cast<float>(face.n[2]));
                    const float light = face.shade * ao[i];
                    const auto c = static_cast<unsigned char>(std::clamp(light * 255.0f, 0.0f, 255.0f));
                    colors.push_back(c);
                    colors.push_back(c);
                    colors.push_back(c);
                    colors.push_back(alpha);
                }
                // Flip the quad split when AO is anisotropic to avoid seams.
                if (ao[0] + ao[2] >= ao[1] + ao[3]) {
                    const unsigned short quad[6] = {base, static_cast<unsigned short>(base + 1),
                                                    static_cast<unsigned short>(base + 2), base,
                                                    static_cast<unsigned short>(base + 2),
                                                    static_cast<unsigned short>(base + 3)};
                    indices.insert(indices.end(), quad, quad + 6);
                } else {
                    const unsigned short quad[6] = {static_cast<unsigned short>(base + 1),
                                                    static_cast<unsigned short>(base + 2),
                                                    static_cast<unsigned short>(base + 3),
                                                    static_cast<unsigned short>(base + 1),
                                                    static_cast<unsigned short>(base + 3), base};
                    indices.insert(indices.end(), quad, quad + 6);
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

        bool occludes(const World& world, const int x, const int y, const int z) {
            return blockInfo(world.block(x, y, z)).opaque;
        }
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
        MeshBuffer opaque;
        MeshBuffer water;

        const int baseX = chunk.key.cx * cfg.chunkSize;
        const int baseZ = chunk.key.cz * cfg.chunkSize;

        for (int y = 0; y < cfg.worldHeight; ++y) {
            for (int lz = 0; lz < cfg.chunkSize; ++lz) {
                for (int lx = 0; lx < cfg.chunkSize; ++lx) {
                    const Block b = chunk.at(lx, y, lz);
                    if (b == Block::Air)
                        continue;
                    const BlockInfo& info = blockInfo(b);
                    const int wx = baseX + lx;
                    const int wz = baseZ + lz;

                    for (const auto& face : kFaces) {
                        const int nx = wx + face.n[0];
                        const int ny = y + face.n[1];
                        const int nz = wz + face.n[2];
                        const Block neighbor = world.block(nx, ny, nz);

                        if (b == Block::Water) {
                            // Water renders only against air (and the sky above).
                            if (neighbor != Block::Air)
                                continue;
                            if (water.full())
                                continue;
                            const float ao[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                            const bool isTop = face.n[1] > 0;
                            water.pushFace(face, static_cast<float>(wx), static_cast<float>(y),
                                           static_cast<float>(wz), tileUV(info.tileTop), ao, 170,
                                           isTop ? 0.86f : 1.0f);
                            continue;
                        }

                        // Opaque + cutout visibility.
                        const BlockInfo& nInfo = blockInfo(neighbor);
                        if (nInfo.opaque)
                            continue;
                        if (info.cutout && neighbor == b)
                            continue; // no internal faces between identical cutout blocks
                        if (opaque.full())
                            continue;

                        // Per-vertex ambient occlusion in the face plane.
                        float ao[4];
                        const int axis = face.n[0] != 0 ? 0 : (face.n[1] != 0 ? 1 : 2);
                        const int t1 = axis == 0 ? 1 : 0;
                        const int t2 = axis == 2 ? 1 : 2;
                        for (int i = 0; i < 4; ++i) {
                            const int corner[3] = {face.c[i][0], face.c[i][1], face.c[i][2]};
                            int d1[3] = {0, 0, 0};
                            int d2[3] = {0, 0, 0};
                            d1[t1] = corner[t1] == 1 ? 1 : -1;
                            d2[t2] = corner[t2] == 1 ? 1 : -1;
                            const int bx = wx + face.n[0];
                            const int by = y + face.n[1];
                            const int bz = wz + face.n[2];
                            const bool s1 = occludes(world, bx + d1[0], by + d1[1], bz + d1[2]);
                            const bool s2 = occludes(world, bx + d2[0], by + d2[1], bz + d2[2]);
                            const bool c = occludes(world, bx + d1[0] + d2[0], by + d1[1] + d2[1],
                                                    bz + d1[2] + d2[2]);
                            const int level = (s1 && s2) ? 0 : 3 - (static_cast<int>(s1) +
                                                                    static_cast<int>(s2) +
                                                                    static_cast<int>(c));
                            ao[i] = kAoLevels[level];
                        }

                        const int tile = face.n[1] > 0 ? info.tileTop
                                       : face.n[1] < 0 ? info.tileBottom
                                                       : info.tileSide;
                        opaque.pushFace(face, static_cast<float>(wx), static_cast<float>(y),
                                        static_cast<float>(wz), tileUV(tile), ao, 255, 1.0f);
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
