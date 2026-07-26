#include "vx_worldgen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "vx_config.hpp"
#include "vx_noise.hpp"
#include "vx_world.hpp"

// ---------------------------------------------------------------------------
// VOXHAVEN world generation.
//
// Every routine here is a pure function of (seed, world coordinate). Nothing
// reads neighbouring chunks, nothing keeps state between calls, and nothing
// depends on the order chunks are generated in. Features bigger than a chunk
// (trees, boulders, landmarks) are produced by enumerating every candidate
// origin in a neighbourhood of chunks and stamping only the part that lands
// inside the chunk being built -- so a tree straddling a border is written
// identically from both sides.
//
// Cost shape (the generator gets ~2 chunks/frame on the main thread):
//   * 256 column samples  -> ~19 value2 evaluations each (the dominant term).
//   * 3 cave fields sampled on a coarse 4x4x4 lattice and trilinearly
//     interpolated instead of one fbm3 per block (~10x cheaper).
//   * feature passes work on a handful of candidate origins, not per block.
// ---------------------------------------------------------------------------

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
        namespace {

            // cfg is a runtime `const` object, so it cannot size stack arrays.
            // These mirror cfg.chunkSize / cfg.worldHeight; keep them in sync.
            constexpr int kCS = 16;
            constexpr int kWH = 96;

            // Distinct salts per noise field. Values are arbitrary but must stay
            // stable or existing saved worlds regenerate differently.
            constexpr std::uint32_t kSaltCont = 1201u;
            constexpr std::uint32_t kSaltEro = 1303u;
            constexpr std::uint32_t kSaltTemp = 1409u;
            constexpr std::uint32_t kSaltHum = 1511u;
            constexpr std::uint32_t kSaltHill = 1613u;
            constexpr std::uint32_t kSaltDetail = 1721u;
            constexpr std::uint32_t kSaltRidge = 1823u;
            constexpr std::uint32_t kSaltRiver = 1931u;
            constexpr std::uint32_t kSaltDune = 2039u;
            constexpr std::uint32_t kSaltJitT = 2141u;
            constexpr std::uint32_t kSaltJitH = 2243u;
            constexpr std::uint32_t kSaltPatch = 2351u;
            constexpr std::uint32_t kSaltCaveA = 2459u;
            constexpr std::uint32_t kSaltCaveB = 2557u;
            constexpr std::uint32_t kSaltCaveC = 2663u;
            constexpr std::uint32_t kSaltCaveD = 2767u;
            constexpr std::uint32_t kSaltEntry = 2879u;
            constexpr std::uint32_t kSaltBed = 2971u;
            constexpr std::uint32_t kSaltVein = 3079u;
            constexpr std::uint32_t kSaltTree = 3187u;
            constexpr std::uint32_t kSaltLeaf = 3299u;
            constexpr std::uint32_t kSaltRock = 3407u;
            constexpr std::uint32_t kSaltRockMat = 3511u;
            constexpr std::uint32_t kSaltStruct = 3623u;
            constexpr std::uint32_t kSaltStructMat = 3739u;

            // ------------------------------------------------------ math helpers

            [[nodiscard]] constexpr float clamp01(const float v) {
                return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            }

            // Hermite ramp; works with e0 > e1 to get a falling edge.
            [[nodiscard]] float smoothstep(const float e0, const float e1, const float x) {
                if (e0 == e1) return x < e0 ? 0.0f : 1.0f;
                const float t = clamp01((x - e0) / (e1 - e0));
                return t * t * (3.0f - 2.0f * t);
            }

            // Value-noise fBM clusters tightly around 0.5 (it is an average of
            // uniforms), so every field is stretched about its midpoint before
            // being used as a control parameter. Without this, thresholds like
            // "cont > 0.7" would essentially never fire.
            [[nodiscard]] float contrast(const float v, const float k) {
                return clamp01((v - 0.5f) * k + 0.5f);
            }

            // Ridged fBM: folding each octave about its midpoint turns smooth
            // hills into sharp creases, which is what reads as a mountain spine.
            [[nodiscard]] float ridged2(const std::uint32_t seed, float x, float y, const int octaves) {
                float sum = 0.0f;
                float amp = 0.5f;
                float total = 0.0f;
                for (int i = 0; i < octaves; ++i) {
                    const float n = noise::value2(seed + static_cast<std::uint32_t>(i) * 137U, x, y);
                    const float r = 1.0f - std::fabs(n * 2.0f - 1.0f);
                    sum += r * r * amp;
                    total += amp;
                    amp *= 0.5f;
                    x *= 2.03f;
                    y *= 2.03f;
                }
                return sum / total;
            }

            // Smooth piecewise spline over an ascending knot table.
            [[nodiscard]] float splineAt(const float* xs, const float* ys, const int n, const float t) {
                if (t <= xs[0]) return ys[0];
                for (int i = 1; i < n; ++i) {
                    if (t <= xs[i]) {
                        const float f = (t - xs[i - 1]) / (xs[i] - xs[i - 1]);
                        return noise::lerp(ys[i - 1], ys[i], noise::smooth(f));
                    }
                }
                return ys[n - 1];
            }

            // ------------------------------------------------------- column model

            // Everything generation needs to know about one world column. This is
            // the single source of truth: biomeAt(), columnHeight() and the chunk
            // filler all read it, so the HUD can never disagree with the blocks.
            struct Column {
                int height = 0;      // topmost terrain block (water excluded)
                Biome biome = Biome::Plains;
                float temp = 0.5f;   // classification temperature (altitude-cooled)
                float humid = 0.5f;
                float cont = 0.5f;   // continentalness
                float river = 0.0f;  // 0..1 river influence
                float mtn = 0.0f;    // mountain-belt mask
                float swampy = 0.0f; // wetland mask, needed to override the shoreline rules
                float patch = 0.5f;  // mid-frequency dither reused by decorators
            };

            [[nodiscard]] Column sampleColumn(const std::uint32_t seed, const int wx, const int wz) {
                const auto fx = static_cast<float>(wx);
                const auto fz = static_cast<float>(wz);

                // Four independent low-frequency control fields.
                const float cont = contrast(noise::fbm2(seed + kSaltCont, fx * 0.0016f, fz * 0.0016f, 3), 1.8f);
                const float ero = contrast(noise::fbm2(seed + kSaltEro, fx * 0.0033f, fz * 0.0033f, 2), 2.0f);
                const float tRaw = contrast(noise::fbm2(seed + kSaltTemp, fx * 0.0011f, fz * 0.0011f, 2), 1.9f);
                const float hRaw = contrast(noise::fbm2(seed + kSaltHum, fx * 0.0014f, fz * 0.0014f, 2), 1.8f);

                // Continentalness -> base elevation. The sea-level crossing sits a
                // little below the modal value of the field so roughly 30% of the
                // world is water, and the knots either side of it are steep so
                // coastlines are narrow strips rather than endless ambiguous flats.
                static constexpr float kContX[] = {0.00f, 0.16f, 0.30f, 0.38f, 0.42f, 0.50f,
                                                   0.60f, 0.74f, 0.88f, 1.00f};
                static constexpr float kContY[] = {12.0f, 17.0f, 23.0f, 29.0f, 32.5f, 36.5f,
                                                   40.0f, 48.0f, 58.0f, 70.0f};
                float h = splineAt(kContX, kContY, 10, cont);

                // Erosion damps the small stuff: high erosion == flat plains,
                // low erosion == rolling, broken ground.
                const float rough = clamp01(1.0f - ero);
                const float hills = noise::fbm2(seed + kSaltHill, fx * 0.011f, fz * 0.011f, 3) * 2.0f - 1.0f;
                const float detail = noise::value2(seed + kSaltDetail, fx * 0.055f, fz * 0.055f) * 2.0f - 1.0f;
                h += hills * (2.5f + 9.0f * rough);
                h += detail * (0.8f + 2.0f * rough);

                // Mountains only where land is both high AND un-eroded, so ranges
                // form coherent belts instead of spikes sprinkled everywhere.
                const float mtn = smoothstep(0.58f, 0.84f, cont) * smoothstep(0.60f, 0.26f, ero);
                if (mtn > 0.01f) {
                    const float r = ridged2(seed + kSaltRidge, fx * 0.0085f, fz * 0.0085f, 4);
                    h += r * r * 32.0f * mtn; // squared -> broad valleys, thin peaks
                }

                // Wind-blown dunes in hot, dry country. Anisotropic frequency
                // gives long crests instead of round lumps.
                const float dryHot = smoothstep(0.56f, 0.74f, tRaw) * smoothstep(0.48f, 0.30f, hRaw);
                if (dryHot > 0.01f) {
                    const float dn = noise::value2(seed + kSaltDune, fx * 0.040f, fz * 0.100f) * 2.0f - 1.0f;
                    h += dn * 4.0f * dryHot;
                }

                // Rivers: the zero contour of a signed field is a long winding
                // line, so |field| near zero traces a channel across the map.
                float river = 0.0f;
                {
                    const float rn = contrast(noise::fbm2(seed + kSaltRiver, fx * 0.0021f, fz * 0.0021f, 2), 2.2f)
                                     * 2.0f - 1.0f;
                    river = 1.0f - smoothstep(0.008f, 0.040f, std::fabs(rn));
                    river *= smoothstep(0.42f, 0.52f, cont);    // no rivers out at sea
                    river *= 1.0f - 0.85f * mtn;                // don't saw ranges in half
                    river *= smoothstep(64.0f, 44.0f, h);       // lowland feature only
                }
                if (river > 0.001f) {
                    // Blending toward min(h, bed) rather than assigning it keeps
                    // the banks sloped instead of leaving a rectangular trench.
                    const float bed = static_cast<float>(cfg.seaLevel) - 1.0f - river * 2.5f;
                    h = noise::lerp(h, std::min(h, bed), river);
                }

                // Swamps sag toward the water table so they end up as a maze of
                // shallow pools rather than dry hills that merely look humid.
                const float swampy = smoothstep(0.62f, 0.80f, hRaw) *
                                     smoothstep(0.30f, 0.40f, tRaw) *
                                     smoothstep(0.74f, 0.62f, tRaw);
                if (swampy > 0.01f) {
                    // Target sits just under the waterline so a good fraction of a
                    // swamp ends up as shallow pools rather than merely damp hills.
                    const float target = static_cast<float>(cfg.seaLevel) - 0.3f;
                    const float nearSea = 1.0f - smoothstep(2.0f, 14.0f, std::fabs(h - target));
                    h = noise::lerp(h, target + detail * 1.6f, swampy * nearSea * 0.92f);
                }

                // Soft knee near the ceiling: a hard clamp would shear peaks into
                // mesas, this just compresses the last stretch.
                if (h > 66.0f) h = 66.0f + (h - 66.0f) * 0.5f;

                Column c;
                c.height = std::clamp(static_cast<int>(std::floor(h)), 2, kWH - 16);
                c.cont = cont;
                c.mtn = mtn;
                c.river = river;
                c.swampy = swampy;

                // Ragged biome borders: a high-frequency wobble on the classifier
                // inputs breaks up the otherwise perfectly smooth isolines.
                const float jitT = (noise::value2(seed + kSaltJitT, fx * 0.21f, fz * 0.21f) - 0.5f) * 0.055f;
                const float jitH = (noise::value2(seed + kSaltJitH, fx * 0.18f, fz * 0.18f) - 0.5f) * 0.055f;
                const float alt = smoothstep(46.0f, 78.0f, static_cast<float>(c.height));
                c.temp = clamp01(tRaw - alt * 0.32f + jitT); // altitude lapse rate
                c.humid = clamp01(hRaw + jitH);
                c.patch = noise::value2(seed + kSaltPatch, fx * 0.085f, fz * 0.085f);

                const int sea = cfg.seaLevel;
                // Swamps are tested first: their pools sit below the waterline and
                // would otherwise be misread as ocean, giving them sand beaches.
                if (swampy > 0.45f && c.height >= sea - 4 && c.height <= sea + 5)
                    c.biome = Biome::Swamp;
                else if (c.height < sea - 1) c.biome = Biome::Ocean;
                else if (c.height <= sea + 1) c.biome = Biome::Beach;
                else if (c.height >= 62 || (c.temp < 0.22f && c.height >= 50)) c.biome = Biome::SnowyPeaks;
                else if (c.temp < 0.33f) c.biome = Biome::Taiga;
                else if (c.temp > 0.68f) c.biome = (c.humid < 0.42f) ? Biome::Desert : Biome::Savanna;
                else if (c.humid > 0.66f && c.height <= sea + 6) c.biome = Biome::Swamp;
                else if (c.humid > 0.50f) c.biome = Biome::Forest;
                else c.biome = Biome::Plains;

                return c;
            }

            // ------------------------------------------------- chunk write helpers

            // All feature stamping goes through these: they clip to the chunk, so
            // a routine can freely write in world coordinates that spill outside.
            void setWorld(Chunk& ch, const int baseX, const int baseZ,
                          const int wx, const int y, const int wz, const Block b) {
                const int lx = wx - baseX;
                const int lz = wz - baseZ;
                if (lx < 0 || lx >= kCS || lz < 0 || lz >= kCS || y < 0 || y >= kWH) return;
                ch.set(lx, y, lz, b);
            }

            [[nodiscard]] Block getWorld(const Chunk& ch, const int baseX, const int baseZ,
                                         const int wx, const int y, const int wz) {
                const int lx = wx - baseX;
                const int lz = wz - baseZ;
                if (lx < 0 || lx >= kCS || lz < 0 || lz >= kCS || y < 0 || y >= kWH) return Block::Air;
                return ch.at(lx, y, lz);
            }

            void setIfAir(Chunk& ch, const int baseX, const int baseZ,
                          const int wx, const int y, const int wz, const Block b) {
                if (getWorld(ch, baseX, baseZ, wx, y, wz) == Block::Air)
                    setWorld(ch, baseX, baseZ, wx, y, wz, b);
            }

            // Trunks may push through air, water and their own foliage, but not
            // through rock a boulder or the terrain already claimed.
            void setTrunk(Chunk& ch, const int baseX, const int baseZ,
                          const int wx, const int y, const int wz) {
                const Block cur = getWorld(ch, baseX, baseZ, wx, y, wz);
                if (cur == Block::Air || cur == Block::Water || cur == Block::Leaves)
                    setWorld(ch, baseX, baseZ, wx, y, wz, Block::Wood);
            }

            // ---------------------------------------------------------- ore veins

            struct VeinSpec {
                Block block;
                std::uint32_t salt;
                int yMin;
                int yMax;
                float density;  // probability a lattice cell spawns a vein
                int minLen;
                int maxLen;
                float radius;
            };

            // Cell size for the vein lattice. Veins wander at most
            // maxLen * kStep blocks from their origin, which stays under one cell
            // so a one-cell halo around the chunk catches every vein that could
            // reach into it.
            constexpr int kVeinCell = 12;
            constexpr float kVeinStep = 1.15f;

            void stampVeinBlob(Chunk& ch, const int baseX, const int baseZ,
                               const float px, const float py, const float pz,
                               const float radius, const Block b) {
                const int r = static_cast<int>(std::ceil(radius));
                const int cx = static_cast<int>(std::floor(px));
                const int cy = static_cast<int>(std::floor(py));
                const int cz = static_cast<int>(std::floor(pz));
                const float r2 = radius * radius;
                for (int dy = -r; dy <= r; ++dy) {
                    const int y = cy + dy;
                    if (y < 1 || y >= kWH) continue;
                    for (int dz = -r; dz <= r; ++dz) {
                        const int wz = cz + dz;
                        if (wz - baseZ < 0 || wz - baseZ >= kCS) continue;
                        for (int dx = -r; dx <= r; ++dx) {
                            const int wx = cx + dx;
                            if (wx - baseX < 0 || wx - baseX >= kCS) continue;
                            const float fdx = static_cast<float>(wx) + 0.5f - px;
                            const float fdy = static_cast<float>(y) + 0.5f - py;
                            const float fdz = static_cast<float>(wz) + 0.5f - pz;
                            if (fdx * fdx + fdy * fdy + fdz * fdz > r2) continue;
                            // Only rock becomes ore: this keeps veins out of caves,
                            // out of the soil layer, and out of the bedrock shell.
                            if (getWorld(ch, baseX, baseZ, wx, y, wz) != Block::Stone) continue;
                            setWorld(ch, baseX, baseZ, wx, y, wz, b);
                        }
                    }
                }
            }

            void placeVeins(Chunk& ch, const std::uint32_t seed, const VeinSpec& spec) {
                const int baseX = ch.key.cx * kCS;
                const int baseZ = ch.key.cz * kCS;
                // Halo of one cell on each side in x/z, and the full depth band.
                const int gx0 = static_cast<int>(std::floor(static_cast<float>(baseX) / kVeinCell)) - 1;
                const int gx1 = static_cast<int>(std::floor(static_cast<float>(baseX + kCS - 1) / kVeinCell)) + 1;
                const int gz0 = static_cast<int>(std::floor(static_cast<float>(baseZ) / kVeinCell)) - 1;
                const int gz1 = static_cast<int>(std::floor(static_cast<float>(baseZ + kCS - 1) / kVeinCell)) + 1;
                const int gy0 = spec.yMin / kVeinCell;
                const int gy1 = spec.yMax / kVeinCell;

                const std::uint32_t s = seed + kSaltVein + spec.salt;
                const float reach = static_cast<float>(spec.maxLen) * kVeinStep + spec.radius + 1.0f;

                for (int gz = gz0; gz <= gz1; ++gz) {
                    for (int gx = gx0; gx <= gx1; ++gx) {
                        for (int gy = gy0; gy <= gy1; ++gy) {
                            if (noise::rand01(s, gx, gy, gz) >= spec.density) continue;

                            const int ox = gx * kVeinCell +
                                           static_cast<int>(noise::rand01(s + 1, gx, gy, gz) * kVeinCell);
                            const int oy = gy * kVeinCell +
                                           static_cast<int>(noise::rand01(s + 2, gx, gy, gz) * kVeinCell);
                            const int oz = gz * kVeinCell +
                                           static_cast<int>(noise::rand01(s + 3, gx, gy, gz) * kVeinCell);
                            if (oy < spec.yMin || oy > spec.yMax) continue;

                            // Cheap bounding reject before doing any trig.
                            if (static_cast<float>(ox) < static_cast<float>(baseX) - reach ||
                                static_cast<float>(ox) > static_cast<float>(baseX + kCS) + reach ||
                                static_cast<float>(oz) < static_cast<float>(baseZ) - reach ||
                                static_cast<float>(oz) > static_cast<float>(baseZ + kCS) + reach)
                                continue;

                            const int len = spec.minLen +
                                            static_cast<int>(noise::rand01(s + 4, gx, gy, gz) *
                                                             static_cast<float>(spec.maxLen - spec.minLen + 1));
                            float yaw = noise::rand01(s + 5, gx, gy, gz) * 6.2831853f;
                            float pitch = (noise::rand01(s + 6, gx, gy, gz) - 0.5f) * 1.4f;
                            float px = static_cast<float>(ox) + 0.5f;
                            float py = static_cast<float>(oy) + 0.5f;
                            float pz = static_cast<float>(oz) + 0.5f;

                            // Short random walk with a blob at every node: the
                            // overlap between consecutive blobs is what makes the
                            // vein one connected lump instead of pepper.
                            for (int step = 0; step < len; ++step) {
                                const float rad = spec.radius *
                                                  (0.72f + 0.45f * noise::rand01(s + 7, ox, oy + step, oz));
                                stampVeinBlob(ch, baseX, baseZ, px, py, pz, rad, spec.block);
                                yaw += (noise::rand01(s + 8, ox + step, oy, oz) - 0.5f) * 1.3f;
                                pitch += (noise::rand01(s + 9, ox, oy, oz + step) - 0.5f) * 0.7f;
                                pitch = std::clamp(pitch, -1.0f, 1.0f);
                                const float cp = std::cos(pitch);
                                px += std::cos(yaw) * cp * kVeinStep;
                                py += std::sin(pitch) * kVeinStep;
                                pz += std::sin(yaw) * cp * kVeinStep;
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------------------- trees

            enum class TreeKind : std::uint8_t { Oak, Pine, Acacia, SwampOak, Dead };

            struct TreeInst {
                int wx = 0;
                int wz = 0;
                int ground = 0;
                TreeKind kind = TreeKind::Oak;
            };

            // Candidate slots per chunk and the largest acceptance probability of
            // any biome; the pre-gate rejects most slots before the (comparatively
            // expensive) column sample.
            constexpr int kTreeSlots = 40;
            constexpr float kTreeMaxP = 0.17f;
            constexpr int kTreeReach = 4;  // max canopy radius in x/z

            // Expected trees per chunk = kTreeSlots * density. Canopies are 5-7
            // blocks wide, so anything past ~7 per chunk closes into an unbroken
            // leaf ceiling with no forest floor to walk on.
            [[nodiscard]] float treeDensity(const Biome b) {
                switch (b) {
                case Biome::Forest: return 0.170f;   // ~6.8 / chunk
                case Biome::Taiga: return 0.130f;    // ~5.2 / chunk
                case Biome::Swamp: return 0.075f;
                case Biome::Savanna: return 0.030f;
                case Biome::SnowyPeaks: return 0.022f;
                case Biome::Plains: return 0.020f;
                case Biome::Desert: return 0.010f;
                default: return 0.0f; // ocean / beach
                }
            }

            [[nodiscard]] TreeKind treeKindFor(const Biome b, const float roll) {
                switch (b) {
                case Biome::Taiga:
                case Biome::SnowyPeaks: return TreeKind::Pine;
                case Biome::Swamp: return TreeKind::SwampOak;
                case Biome::Savanna: return roll < 0.75f ? TreeKind::Acacia : TreeKind::Dead;
                case Biome::Desert: return TreeKind::Dead;
                case Biome::Plains: return roll < 0.85f ? TreeKind::Oak : TreeKind::Dead;
                default: return TreeKind::Oak;
                }
            }

            [[nodiscard]] int trunkHeightFor(const std::uint32_t seed, const TreeInst& t) {
                const float r = noise::rand01(seed + kSaltTree + 11, t.wx, t.wz, 1);
                switch (t.kind) {
                case TreeKind::Pine: return 7 + static_cast<int>(r * 6.0f);
                case TreeKind::Acacia: return 4 + static_cast<int>(r * 3.0f);
                case TreeKind::SwampOak: return 5 + static_cast<int>(r * 3.0f);
                case TreeKind::Dead: return 3 + static_cast<int>(r * 3.0f);
                default: return 5 + static_cast<int>(r * 4.0f);
                }
            }

            // Acacias lean; the offset is shared by the trunk and canopy passes so
            // both agree without storing anything.
            void acaciaLean(const std::uint32_t seed, const TreeInst& t, int& dx, int& dz) {
                static constexpr int kLx[] = {-1, 1, 0, 0};
                static constexpr int kLz[] = {0, 0, -1, 1};
                const int k = static_cast<int>(noise::rand01(seed + kSaltTree + 12, t.wx, t.wz, 2) * 4.0f);
                dx = kLx[std::clamp(k, 0, 3)];
                dz = kLz[std::clamp(k, 0, 3)];
            }

            void stampTrunk(Chunk& ch, const int baseX, const int baseZ,
                            const std::uint32_t seed, const TreeInst& t) {
                const int th = trunkHeightFor(seed, t);
                if (t.kind == TreeKind::Acacia) {
                    int lx = 0;
                    int lz = 0;
                    acaciaLean(seed, t, lx, lz);
                    const int straight = th - 2;
                    for (int i = 1; i <= straight; ++i)
                        setTrunk(ch, baseX, baseZ, t.wx, t.ground + i, t.wz);
                    setTrunk(ch, baseX, baseZ, t.wx + lx, t.ground + straight + 1, t.wz + lz);
                    setTrunk(ch, baseX, baseZ, t.wx + lx, t.ground + straight + 2, t.wz + lz);
                    return;
                }
                for (int i = 1; i <= th; ++i)
                    setTrunk(ch, baseX, baseZ, t.wx, t.ground + i, t.wz);

                if (t.kind == TreeKind::Dead) {
                    // A couple of bare branches so it reads as a snag, not a post.
                    const int by = t.ground + th - 1;
                    if (noise::rand01(seed + kSaltTree + 13, t.wx, t.wz, 3) < 0.7f)
                        setTrunk(ch, baseX, baseZ, t.wx + 1, by, t.wz);
                    if (noise::rand01(seed + kSaltTree + 14, t.wx, t.wz, 4) < 0.7f)
                        setTrunk(ch, baseX, baseZ, t.wx - 1, by + 1, t.wz);
                    if (noise::rand01(seed + kSaltTree + 15, t.wx, t.wz, 5) < 0.5f)
                        setTrunk(ch, baseX, baseZ, t.wx, by, t.wz + 1);
                } else if (t.kind == TreeKind::SwampOak) {
                    // Buttress roots at the waterline.
                    for (int k = 0; k < 4; ++k) {
                        static constexpr int kOx[] = {1, -1, 0, 0};
                        static constexpr int kOz[] = {0, 0, 1, -1};
                        if (noise::rand01(seed + kSaltTree + 16, t.wx + kOx[k], t.wz + kOz[k], 6) < 0.55f)
                            setTrunk(ch, baseX, baseZ, t.wx + kOx[k], t.ground + 1, t.wz + kOz[k]);
                    }
                }
            }

            // Leaf placement is keyed purely on absolute block position, so two
            // overlapping canopies make the same decision for a shared block and
            // the result is independent of which tree is stamped first.
            void leafBlob(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                          const int cx, const int cy, const int cz,
                          const int rx, const int ry, const float density) {
                for (int dy = -ry; dy <= ry; ++dy) {
                    for (int dz = -rx; dz <= rx; ++dz) {
                        for (int dx = -rx; dx <= rx; ++dx) {
                            const float fx = static_cast<float>(dx) / static_cast<float>(rx + 1);
                            const float fz = static_cast<float>(dz) / static_cast<float>(rx + 1);
                            const float fy = static_cast<float>(dy) / static_cast<float>(ry + 1);
                            if (fx * fx + fz * fz + fy * fy > 1.0f) continue;
                            if (noise::rand01(seed + kSaltLeaf, cx + dx, cy + dy, cz + dz) > density) continue;
                            setIfAir(ch, baseX, baseZ, cx + dx, cy + dy, cz + dz, Block::Leaves);
                        }
                    }
                }
            }

            void leafDisc(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                          const int cx, const int cy, const int cz, const int r, const float density) {
                const int r2 = r * r + r;
                for (int dz = -r; dz <= r; ++dz) {
                    for (int dx = -r; dx <= r; ++dx) {
                        if (dx * dx + dz * dz > r2) continue;
                        if (noise::rand01(seed + kSaltLeaf, cx + dx, cy, cz + dz) > density) continue;
                        setIfAir(ch, baseX, baseZ, cx + dx, cy, cz + dz, Block::Leaves);
                    }
                }
            }

            void stampCanopy(Chunk& ch, const int baseX, const int baseZ,
                             const std::uint32_t seed, const TreeInst& t) {
                const int th = trunkHeightFor(seed, t);
                const float v = noise::rand01(seed + kSaltTree + 21, t.wx, t.wz, 7);

                switch (t.kind) {
                case TreeKind::Dead:
                    return; // no foliage by definition

                case TreeKind::Pine: {
                    // Conical: radius falls off with height, with a one-block
                    // sawtooth so the silhouette is spiky rather than a smooth cone.
                    const int top = t.ground + th + 1;
                    const int bottom = t.ground + 2;
                    for (int y = bottom; y <= top; ++y) {
                        const int up = top - y;
                        int r = static_cast<int>(static_cast<float>(up) * 0.38f);
                        if ((up & 1) == 1) r += 1;
                        r = std::clamp(r, 0, 3);
                        leafDisc(ch, baseX, baseZ, seed, t.wx, y, t.wz, r, 0.80f + v * 0.18f);
                    }
                    return;
                }

                case TreeKind::Acacia: {
                    int lx = 0;
                    int lz = 0;
                    acaciaLean(seed, t, lx, lz);
                    const int cy = t.ground + th + 1;
                    // Flat, wide, thin crown -- the acacia signature.
                    leafDisc(ch, baseX, baseZ, seed, t.wx + lx, cy, t.wz + lz, 3, 0.72f + v * 0.2f);
                    leafDisc(ch, baseX, baseZ, seed, t.wx + lx, cy + 1, t.wz + lz, 2, 0.6f + v * 0.2f);
                    return;
                }

                case TreeKind::SwampOak: {
                    const int cy = t.ground + th;
                    const int r = 3 + (v > 0.6f ? 1 : 0);
                    leafBlob(ch, baseX, baseZ, seed, t.wx, cy, t.wz, r, 2, 0.55f + v * 0.2f);
                    // Curtains of foliage hanging off the rim.
                    for (int dz = -r; dz <= r; ++dz) {
                        for (int dx = -r; dx <= r; ++dx) {
                            if (dx * dx + dz * dz < (r - 1) * (r - 1)) continue;
                            if (dx * dx + dz * dz > r * r + r) continue;
                            const float hang = noise::rand01(seed + kSaltLeaf + 3, t.wx + dx, cy, t.wz + dz);
                            if (hang > 0.35f) continue;
                            const int drop = 1 + static_cast<int>(hang * 6.0f);
                            for (int k = 1; k <= drop; ++k)
                                setIfAir(ch, baseX, baseZ, t.wx + dx, cy - 2 - k, t.wz + dz, Block::Leaves);
                        }
                    }
                    return;
                }

                default: {
                    // Oak: broad rounded canopy sat on the top of the trunk.
                    const int cy = t.ground + th;
                    const int r = 2 + (v > 0.75f ? 1 : 0);
                    leafBlob(ch, baseX, baseZ, seed, t.wx, cy, t.wz, r, 2, 0.62f + v * 0.30f);
                    leafDisc(ch, baseX, baseZ, seed, t.wx, cy + 2, t.wz, 1, 0.9f);
                    return;
                }
                }
            }

            // ----------------------------------------------------------- boulders

            constexpr int kRockSlots = 2;
            constexpr int kRockReach = 4;

            void stampBoulder(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                              const int wx, const int wz, const int ground) {
                const float rx = 1.6f + noise::rand01(seed + kSaltRock + 1, wx, wz, 1) * 1.7f;
                const float ry = 1.3f + noise::rand01(seed + kSaltRock + 2, wx, wz, 2) * 1.5f;
                const float rz = 1.6f + noise::rand01(seed + kSaltRock + 3, wx, wz, 3) * 1.7f;
                const float cy = static_cast<float>(ground) + ry * 0.45f;
                const int ir = static_cast<int>(std::ceil(std::max(rx, rz)));
                const int iy = static_cast<int>(std::ceil(ry));
                for (int dy = -iy; dy <= iy; ++dy) {
                    for (int dz = -ir; dz <= ir; ++dz) {
                        for (int dx = -ir; dx <= ir; ++dx) {
                            const float ax = static_cast<float>(dx) / rx;
                            const float az = static_cast<float>(dz) / rz;
                            const float ay = (static_cast<float>(ground + dy) - cy) / ry;
                            if (ax * ax + ay * ay + az * az > 1.0f) continue;
                            // Material is a function of the block position, not of
                            // the boulder, so overlapping boulders still agree.
                            const Block m = noise::rand01(seed + kSaltRockMat, wx + dx, ground + dy, wz + dz) < 0.55f
                                                ? Block::Cobble
                                                : Block::Stone;
                            setIfAir(ch, baseX, baseZ, wx + dx, ground + dy, wz + dz, m);
                        }
                    }
                }
            }

            // --------------------------------------------------------- structures

            // One landmark candidate per kStructCell square. Placement is confined
            // to the middle of the cell (kStructMargin), which guarantees two
            // landmarks can never be closer than 2*margin - 2*reach blocks and
            // therefore can never overlap -- so their unconditional writes stay
            // order-independent.
            constexpr int kStructCell = 176;
            constexpr int kStructMargin = 30;
            constexpr int kStructReach = 7;

            [[nodiscard]] Block ruinMaterial(const std::uint32_t seed, const int wx, const int y, const int wz) {
                const float m = noise::rand01(seed + kSaltStructMat, wx, y, wz);
                if (m < 0.55f) return Block::Cobble;
                if (m < 0.90f) return Block::Bricks;
                return Block::Stone;
            }

            void buildRuinedShelter(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                                    const int wx, const int wz, const int base) {
                const std::uint32_t s = seed + kSaltStruct + 101u;
                constexpr int kR = 3;
                // Foundation: two courses below the floor bridge any small dip so
                // the ruin never floats over uneven ground.
                for (int dz = -kR; dz <= kR; ++dz) {
                    for (int dx = -kR; dx <= kR; ++dx) {
                        for (int y = base - 2; y <= base; ++y)
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Cobble);
                        for (int y = base + 1; y <= base + 6; ++y)
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Air);
                    }
                }
                // Plank flooring inside the walls.
                for (int dz = -kR + 1; dz <= kR - 1; ++dz)
                    for (int dx = -kR + 1; dx <= kR - 1; ++dx)
                        setWorld(ch, baseX, baseZ, wx + dx, base, wz + dz, Block::Planks);

                // Perimeter walls, collapsing more the higher they go.
                for (int dz = -kR; dz <= kR; ++dz) {
                    for (int dx = -kR; dx <= kR; ++dx) {
                        if (std::abs(dx) != kR && std::abs(dz) != kR) continue;
                        const int wallH = 3 + static_cast<int>(noise::rand01(s, wx + dx, wz + dz, 1) * 2.0f);
                        for (int y = 1; y <= wallH; ++y) {
                            const float gone = noise::rand01(s + 1, wx + dx, base + y, wz + dz);
                            if (gone < 0.10f + static_cast<float>(y) * 0.11f) continue;
                            setWorld(ch, baseX, baseZ, wx + dx, base + y, wz + dz,
                                     ruinMaterial(seed, wx + dx, base + y, wz + dz));
                        }
                    }
                }
                // Doorway punched through the south wall.
                for (int y = 1; y <= 2; ++y) {
                    setWorld(ch, baseX, baseZ, wx, base + y, wz + kR, Block::Air);
                    setWorld(ch, baseX, baseZ, wx - 1, base + y, wz + kR, Block::Air);
                }
                // A single light source, so the ruin is visible from far off at night.
                setWorld(ch, baseX, baseZ, wx + 1, base + 1, wz - 1, Block::Lantern);
                setWorld(ch, baseX, baseZ, wx - 2, base + 1, wz + 1, Block::Planks);
            }

            void buildStoneCircle(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                                  const int wx, const int wz, const int base) {
                const std::uint32_t s = seed + kSaltStruct + 202u;
                constexpr int kN = 8;
                for (int k = 0; k < kN; ++k) {
                    const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(kN);
                    const int px = wx + static_cast<int>(std::lround(std::cos(a) * 5.0f));
                    const int pz = wz + static_cast<int>(std::lround(std::sin(a) * 5.0f));
                    // Each menhir stands on its own column so the ring follows the
                    // ground instead of hovering.
                    const int g = sampleColumn(seed, px, pz).height;
                    const int hgt = 3 + static_cast<int>(noise::rand01(s, px, pz, k) * 3.0f);
                    for (int y = 0; y <= hgt; ++y) {
                        const Block m = noise::rand01(s + 1, px, g + y, pz) < 0.7f ? Block::Stone : Block::Cobble;
                        setWorld(ch, baseX, baseZ, px, g + y, pz, m);
                    }
                    // Lean a fallen capstone off every other pillar.
                    if ((k & 1) == 0)
                        setWorld(ch, baseX, baseZ, px, g + hgt + 1, pz, Block::Cobble);
                }
                // Altar: a glowing stone ringed with cobble.
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx)
                        setWorld(ch, baseX, baseZ, wx + dx, base, wz + dz, Block::Cobble);
                setWorld(ch, baseX, baseZ, wx, base, wz, Block::Glowstone);
            }

            void buildCamp(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                           const int wx, const int wz, const int base) {
                const std::uint32_t s = seed + kSaltStruct + 303u;
                // Clear and level a small pad. The two courses below the surface
                // stop the pad hanging in the air over a dip in the ground.
                for (int dz = -3; dz <= 3; ++dz) {
                    for (int dx = -3; dx <= 3; ++dx) {
                        for (int y = base - 2; y <= base; ++y)
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Dirt);
                        for (int y = base + 1; y <= base + 5; ++y)
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Air);
                    }
                }
                // Plank tent floor with four corner posts and a flat roof.
                for (int dz = -2; dz <= 0; ++dz)
                    for (int dx = -2; dx <= 0; ++dx)
                        setWorld(ch, baseX, baseZ, wx + dx, base, wz + dz, Block::Planks);
                for (int k = 0; k < 4; ++k) {
                    static constexpr int kPx[] = {-2, 0, -2, 0};
                    static constexpr int kPz[] = {-2, -2, 0, 0};
                    for (int y = 1; y <= 2; ++y)
                        setWorld(ch, baseX, baseZ, wx + kPx[k], base + y, wz + kPz[k], Block::Wood);
                }
                for (int dz = -2; dz <= 0; ++dz)
                    for (int dx = -2; dx <= 0; ++dx)
                        setWorld(ch, baseX, baseZ, wx + dx, base + 3, wz + dz, Block::Planks);
                setWorld(ch, baseX, baseZ, wx - 1, base + 2, wz - 1, Block::Lantern);

                // Campfire: a cobble ring with embers still glowing in it.
                for (int dz = -1; dz <= 1; ++dz)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dx != 0 || dz != 0)
                            setWorld(ch, baseX, baseZ, wx + 2 + dx, base, wz + 2 + dz, Block::Cobble);
                setWorld(ch, baseX, baseZ, wx + 2, base, wz + 2, Block::Glowstone);

                // Scattered firewood.
                for (int k = 0; k < 5; ++k) {
                    const int ox = static_cast<int>(noise::rand01(s, wx, wz, k) * 7.0f) - 3;
                    const int oz = static_cast<int>(noise::rand01(s + 1, wx, wz, k) * 7.0f) - 3;
                    setWorld(ch, baseX, baseZ, wx + ox, base + 1, wz + oz, Block::Wood);
                }
            }

            void buildCaveEntrance(Chunk& ch, const int baseX, const int baseZ, const std::uint32_t seed,
                                   const int wx, const int wz, const int base) {
                const std::uint32_t s = seed + kSaltStruct + 404u;
                const int depth = 14 + static_cast<int>(noise::rand01(s, wx, wz, 1) * 10.0f);
                const int bottom = std::max(6, base - depth);

                // Cobble collar so the mouth reads as built, not as a sinkhole.
                for (int dz = -2; dz <= 2; ++dz)
                    for (int dx = -2; dx <= 2; ++dx)
                        for (int y = base - 1; y <= base; ++y)
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Cobble);

                for (int y = bottom; y <= base + 1; ++y) {
                    // Lined 2x2 shaft.
                    for (int dz = -1; dz <= 2; ++dz) {
                        for (int dx = -1; dx <= 2; ++dx) {
                            const bool inner = (dx >= 0 && dx <= 1 && dz >= 0 && dz <= 1);
                            if (inner) {
                                setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Air);
                            } else if (y <= base) {
                                setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Cobble);
                            }
                        }
                    }
                    // A lantern set into the lining every few courses.
                    if (y > bottom && ((base - y) % 4) == 1)
                        setWorld(ch, baseX, baseZ, wx - 1, y, wz, Block::Lantern);
                }

                // Small chamber at the foot of the shaft; it frequently breaks into
                // the natural cave network, which is the point of the landmark.
                for (int dz = -2; dz <= 3; ++dz) {
                    for (int dx = -2; dx <= 3; ++dx) {
                        for (int y = bottom; y <= bottom + 2; ++y) {
                            if (dx * dx + dz * dz > 11) continue;
                            setWorld(ch, baseX, baseZ, wx + dx, y, wz + dz, Block::Air);
                        }
                    }
                }
                setWorld(ch, baseX, baseZ, wx + 2, bottom, wz + 2, Block::Glowstone);
                setWorld(ch, baseX, baseZ, wx - 1, bottom, wz - 1, Block::Glowstone);
            }

            void placeStructures(Chunk& ch, const std::uint32_t seed) {
                const int baseX = ch.key.cx * kCS;
                const int baseZ = ch.key.cz * kCS;
                const std::uint32_t s = seed + kSaltStruct;

                const int gx0 = static_cast<int>(
                    std::floor(static_cast<float>(baseX - kStructReach) / kStructCell));
                const int gx1 = static_cast<int>(
                    std::floor(static_cast<float>(baseX + kCS - 1 + kStructReach) / kStructCell));
                const int gz0 = static_cast<int>(
                    std::floor(static_cast<float>(baseZ - kStructReach) / kStructCell));
                const int gz1 = static_cast<int>(
                    std::floor(static_cast<float>(baseZ + kCS - 1 + kStructReach) / kStructCell));
                constexpr int kSpan = kStructCell - 2 * kStructMargin;

                for (int gz = gz0; gz <= gz1; ++gz) {
                    for (int gx = gx0; gx <= gx1; ++gx) {
                        if (noise::rand01(s, gx, gz, 0) > 0.80f) continue;

                        const int wx = gx * kStructCell + kStructMargin +
                                       static_cast<int>(noise::rand01(s + 1, gx, gz, 0) * kSpan);
                        const int wz = gz * kStructCell + kStructMargin +
                                       static_cast<int>(noise::rand01(s + 2, gx, gz, 0) * kSpan);

                        // Reject before touching any noise if it cannot reach here.
                        if (wx + kStructReach < baseX || wx - kStructReach >= baseX + kCS) continue;
                        if (wz + kStructReach < baseZ || wz - kStructReach >= baseZ + kCS) continue;

                        // Suitability: needs dry, roughly level ground.
                        const Column mid = sampleColumn(seed, wx, wz);
                        if (mid.height < cfg.seaLevel + 5) continue;
                        if (mid.biome == Biome::Ocean || mid.biome == Biome::Beach ||
                            mid.biome == Biome::Swamp)
                            continue;
                        int lo = mid.height;
                        int hi = mid.height;
                        for (int k = 0; k < 4; ++k) {
                            static constexpr int kCx[] = {-5, 5, -5, 5};
                            static constexpr int kCz[] = {-5, -5, 5, 5};
                            const int gh = sampleColumn(seed, wx + kCx[k], wz + kCz[k]).height;
                            lo = std::min(lo, gh);
                            hi = std::max(hi, gh);
                        }
                        if (hi - lo > 3) continue;

                        const int base = mid.height;
                        const int kind = static_cast<int>(noise::rand01(s + 3, gx, gz, 0) * 4.0f);
                        switch (kind) {
                        case 0: buildRuinedShelter(ch, baseX, baseZ, seed, wx, wz, base); break;
                        case 1: buildStoneCircle(ch, baseX, baseZ, seed, wx, wz, base); break;
                        case 2: buildCamp(ch, baseX, baseZ, seed, wx, wz, base); break;
                        default: buildCaveEntrance(ch, baseX, baseZ, seed, wx, wz, base); break;
                        }
                    }
                }
            }

            // -------------------------------------------------------- cave fields

            // The cave noise is smooth on a scale of tens of blocks, so evaluating
            // it per block is pure waste. It is sampled on a 4x4x4 lattice over the
            // chunk and trilinearly interpolated. The two tunnel fields are stored
            // *signed*: interpolating first and taking |v| afterwards keeps the
            // zero-crossing crease (which is the tunnel) sharp.
            constexpr int kCaveStep = 4;
            constexpr int kCaveNX = kCS / kCaveStep + 1;   // 5
            constexpr int kCaveNY = kWH / kCaveStep + 2;   // 26

            struct CaveLattice {
                std::array<float, kCaveNX * kCaveNX * kCaveNY> a{};
                std::array<float, kCaveNX * kCaveNX * kCaveNY> b{};
                std::array<float, kCaveNX * kCaveNX * kCaveNY> c{};
                int levels = 0;
            };

            void buildCaveLattice(CaveLattice& L, const std::uint32_t seed,
                                  const int baseX, const int baseZ, const int topY) {
                L.levels = std::min(kCaveNY, topY / kCaveStep + 2);
                for (int j = 0; j < L.levels; ++j) {
                    const auto fy = static_cast<float>(j * kCaveStep);
                    for (int k = 0; k < kCaveNX; ++k) {
                        const auto fz = static_cast<float>(baseZ + k * kCaveStep);
                        for (int i = 0; i < kCaveNX; ++i) {
                            const auto fx = static_cast<float>(baseX + i * kCaveStep);
                            const int idx = (j * kCaveNX + k) * kCaveNX + i;
                            // Higher y frequency than x/z -> tunnels run flatter,
                            // the way real cave systems tend to.
                            L.a[static_cast<std::size_t>(idx)] =
                                noise::fbm3(seed + kSaltCaveA, fx * 0.0125f, fy * 0.020f, fz * 0.0125f, 2)
                                    * 2.0f - 1.0f;
                            L.b[static_cast<std::size_t>(idx)] =
                                noise::fbm3(seed + kSaltCaveB, fx * 0.0138f, fy * 0.022f, fz * 0.0138f, 2)
                                    * 2.0f - 1.0f;
                            L.c[static_cast<std::size_t>(idx)] =
                                noise::fbm3(seed + kSaltCaveC, fx * 0.0180f, fy * 0.036f, fz * 0.0180f, 2)
                                    * 2.0f - 1.0f;
                        }
                    }
                }
            }

        } // namespace

        // ------------------------------------------------------- public interface

        int columnHeight(const std::uint32_t seed, const int worldX, const int worldZ) {
            return sampleColumn(seed, worldX, worldZ).height;
        }

        Biome biomeAt(const std::uint32_t seed, const int worldX, const int worldZ) {
            return sampleColumn(seed, worldX, worldZ).biome;
        }

        void generateChunk(Chunk& chunk, const std::uint32_t seed) {
            const int baseX = chunk.key.cx * kCS;
            const int baseZ = chunk.key.cz * kCS;
            const int sea = cfg.seaLevel;

            // ---- pass 1: per-column terrain model -------------------------------
            std::array<Column, kCS * kCS> cols{};
            std::array<int, kCS * kCS> caveCeil{};
            std::array<float, kCS * kCS> caveMul{};
            int maxCaveTop = 0;

            for (int lz = 0; lz < kCS; ++lz) {
                for (int lx = 0; lx < kCS; ++lx) {
                    const int wx = baseX + lx;
                    const int wz = baseZ + lz;
                    const auto fx = static_cast<float>(wx);
                    const auto fz = static_cast<float>(wz);
                    const std::size_t ci = static_cast<std::size_t>(lz) * kCS + static_cast<std::size_t>(lx);
                    const Column c = sampleColumn(seed, wx, wz);
                    cols[ci] = c;

                    // How close to the surface caves are allowed to reach. Water
                    // bodies get a much thicker cap so nothing is ever carved out
                    // from under a lake or sea floor.
                    int ceil;
                    if (c.height <= sea + 1) ceil = std::min(c.height - 6, sea - 6);
                    else if (c.height <= sea + 6) ceil = std::min(c.height - 5, sea - 4);
                    else ceil = c.height - 5;

                    // Rare surface openings: without them a layered cave system is
                    // sealed and effectively does not exist for the player.
                    if (c.height > sea + 10) {
                        const float e = noise::value2(seed + kSaltEntry, fx * 0.035f, fz * 0.035f);
                        if (e > 0.86f) ceil = c.height;
                    }
                    caveCeil[ci] = ceil;
                    maxCaveTop = std::max(maxCaveTop, ceil);

                    // Regional cave density: some areas are honeycombed, others
                    // nearly solid, which makes exploring feel less uniform.
                    caveMul[ci] = 0.65f + 0.80f * noise::value2(seed + kSaltCaveD, fx * 0.006f, fz * 0.006f);
                }
            }

            CaveLattice lattice;
            if (maxCaveTop > 4)
                buildCaveLattice(lattice, seed, baseX, baseZ, std::min(maxCaveTop, kWH - 1));

            // ---- pass 2: fill columns ------------------------------------------
            std::array<float, kCaveNY> ca{};
            std::array<float, kCaveNY> cb{};
            std::array<float, kCaveNY> cc{};

            for (int lz = 0; lz < kCS; ++lz) {
                for (int lx = 0; lx < kCS; ++lx) {
                    const std::size_t ci = static_cast<std::size_t>(lz) * kCS + static_cast<std::size_t>(lx);
                    const Column& c = cols[ci];
                    const int wx = baseX + lx;
                    const int wz = baseZ + lz;
                    const int h = c.height;

                    // ---- surface / subsurface materials
                    Block topB = Block::Grass;
                    Block subB = Block::Dirt;
                    int subDepth = 4;
                    switch (c.biome) {
                    case Biome::Ocean:
                        topB = Block::Sand;
                        subB = Block::Sand;
                        subDepth = 3;
                        if (c.patch > 0.76f) {
                            topB = Block::Gravel;
                            subB = Block::Gravel;
                        } else if (h >= sea - 8 && c.patch < 0.28f) {
                            // Clay likes shallow, still water.
                            topB = Block::Clay;
                            subB = Block::Clay;
                            subDepth = 2;
                        }
                        break;
                    case Biome::Beach:
                        topB = Block::Sand;
                        subB = Block::Sand;
                        subDepth = 4;
                        if (c.patch > 0.74f) {
                            topB = Block::Gravel;
                            subB = Block::Gravel;
                        }
                        break;
                    case Biome::Desert:
                        topB = Block::Sand;
                        subB = Block::Sand;
                        subDepth = 6;
                        break;
                    case Biome::Savanna:
                    case Biome::Taiga:
                        subDepth = 3;
                        break;
                    case Biome::Swamp:
                        subDepth = 4;
                        if (h <= sea && c.patch < 0.45f) {
                            topB = Block::Clay;
                            subB = Block::Clay;
                        }
                        break;
                    case Biome::SnowyPeaks:
                        topB = Block::Snow;
                        subB = Block::Stone;
                        subDepth = 2;
                        break;
                    default:
                        break;
                    }

                    // Altitude snow line, dithered so the edge is not a contour map.
                    const float snowLine = 52.0f + (c.temp - 0.5f) * 60.0f + (c.patch - 0.5f) * 6.0f;
                    if (h > sea + 1 && static_cast<float>(h) > snowLine) {
                        topB = Block::Snow;
                        if (static_cast<float>(h) > snowLine + 8.0f) {
                            subB = Block::Stone;
                            subDepth = 2;
                        }
                    } else if (c.patch > 0.82f &&
                               (c.biome == Biome::Plains || c.biome == Biome::Taiga ||
                                c.biome == Biome::Savanna)) {
                        topB = Block::Gravel; // scattered scree patches
                    }

                    // Cold, shallow water freezes over.
                    const bool frozen = (h < sea) && (c.temp < 0.28f);

                    // ---- bilinear slice of the cave lattice for this column
                    const int i0 = lx / kCaveStep;
                    const int k0 = lz / kCaveStep;
                    const float tx = static_cast<float>(lx % kCaveStep) / kCaveStep;
                    const float tz = static_cast<float>(lz % kCaveStep) / kCaveStep;
                    const int ceilY = caveCeil[ci];
                    const bool anyCaves = lattice.levels > 0 && ceilY > 4;
                    if (anyCaves) {
                        for (int j = 0; j < lattice.levels; ++j) {
                            const std::size_t bIdx =
                                static_cast<std::size_t>((j * kCaveNX + k0) * kCaveNX + i0);
                            const std::size_t r = static_cast<std::size_t>(kCaveNX);
                            ca[static_cast<std::size_t>(j)] = noise::lerp(
                                noise::lerp(lattice.a[bIdx], lattice.a[bIdx + 1], tx),
                                noise::lerp(lattice.a[bIdx + r], lattice.a[bIdx + r + 1], tx), tz);
                            cb[static_cast<std::size_t>(j)] = noise::lerp(
                                noise::lerp(lattice.b[bIdx], lattice.b[bIdx + 1], tx),
                                noise::lerp(lattice.b[bIdx + r], lattice.b[bIdx + r + 1], tx), tz);
                            cc[static_cast<std::size_t>(j)] = noise::lerp(
                                noise::lerp(lattice.c[bIdx], lattice.c[bIdx + 1], tx),
                                noise::lerp(lattice.c[bIdx + r], lattice.c[bIdx + r + 1], tx), tz);
                        }
                    }

                    // ---- write the column
                    const int top = std::max(h, sea);
                    for (int y = top + 1; y < kWH; ++y)
                        chunk.set(lx, y, lz, Block::Air);

                    const float mul = caveMul[ci];
                    for (int y = 0; y <= top; ++y) {
                        Block b;
                        if (y == 0) {
                            b = Block::Bedrock;
                        } else if (y <= 3 &&
                                   noise::rand01(seed + kSaltBed, wx, y, wz) <
                                       static_cast<float>(4 - y) * 0.30f) {
                            b = Block::Bedrock; // ragged bedrock shell, never carved
                        } else if (y < h - subDepth) {
                            b = Block::Stone;
                        } else if (y < h) {
                            b = subB;
                        } else if (y == h) {
                            b = topB;
                        } else if (y == sea && frozen) {
                            b = Block::Ice;
                        } else {
                            b = Block::Water;
                        }

                        // ---- caves
                        if (anyCaves && y > 3 && y <= ceilY && b != Block::Air &&
                            b != Block::Bedrock && b != Block::Water && b != Block::Ice) {
                            const int j = y / kCaveStep;
                            if (j + 1 < lattice.levels) {
                                const float ty = static_cast<float>(y % kCaveStep) / kCaveStep;
                                const auto jj = static_cast<std::size_t>(j);
                                const float s1 = noise::lerp(ca[jj], ca[jj + 1], ty);
                                const float s2 = noise::lerp(cb[jj], cb[jj + 1], ty);
                                // Tunnels taper to nothing as they approach the
                                // ceiling, so the surface is not swiss cheese.
                                const float ramp = clamp01(static_cast<float>(ceilY - y) * 0.14f);
                                const float thr = (0.017f + 0.031f * ramp) * mul;
                                if (std::fabs(s1) < thr && std::fabs(s2) < thr) {
                                    b = Block::Air;
                                } else if (ramp > 0.95f && y < 40) {
                                    // Third field: occasional large caverns, deep only.
                                    const float s3 = noise::lerp(cc[jj], cc[jj + 1], ty);
                                    if (s3 > 0.46f + (1.0f - mul * 0.7f) * 0.10f) b = Block::Air;
                                }
                            }
                        }

                        chunk.set(lx, y, lz, b);
                    }
                }
            }

            // ---- pass 3: ore and sediment veins ---------------------------------
            // Depth bands overlap deliberately; the fixed pass order decides who
            // wins where two veins cross, which keeps the result order-free.
            static constexpr VeinSpec kVeins[] = {
                {Block::Gravel, 0u, 4, 68, 0.10f, 3, 5, 2.2f},
                {Block::CoalOre, 40u, 6, 70, 0.35f, 4, 8, 1.6f},
                {Block::IronOre, 80u, 4, 44, 0.25f, 3, 6, 1.35f},
                {Block::GoldOre, 120u, 2, 16, 0.10f, 2, 4, 1.15f},
            };
            for (const VeinSpec& v : kVeins)
                placeVeins(chunk, seed, v);

            // ---- pass 4: boulders (cross-chunk) ---------------------------------
            // For every chunk in a 3x3 neighbourhood, ask which boulders originate
            // there and stamp only the overlap. The origin is discarded early if it
            // is too far away to reach this chunk, before any column sampling.
            for (int ncz = chunk.key.cz - 1; ncz <= chunk.key.cz + 1; ++ncz) {
                for (int ncx = chunk.key.cx - 1; ncx <= chunk.key.cx + 1; ++ncx) {
                    for (int i = 0; i < kRockSlots; ++i) {
                        const int wx = ncx * kCS +
                                       static_cast<int>(noise::rand01(seed + kSaltRock, ncx, ncz, i) * kCS);
                        const int wz = ncz * kCS +
                                       static_cast<int>(noise::rand01(seed + kSaltRock + 7, ncx, ncz, i) * kCS);
                        if (wx + kRockReach < baseX || wx - kRockReach >= baseX + kCS) continue;
                        if (wz + kRockReach < baseZ || wz - kRockReach >= baseZ + kCS) continue;
                        if (noise::rand01(seed + kSaltRock + 13, ncx, ncz, i) >= 0.22f) continue;

                        const Column c = sampleColumn(seed, wx, wz);
                        if (c.height <= sea + 1) continue;
                        if (c.biome == Biome::Ocean || c.biome == Biome::Beach ||
                            c.biome == Biome::Swamp || c.biome == Biome::Desert)
                            continue;
                        stampBoulder(chunk, baseX, baseZ, seed, wx, wz, c.height);
                    }
                }
            }

            // ---- pass 5: trees (cross-chunk, two sub-passes) --------------------
            // Trunks are written before any leaves so that overlapping canopies can
            // never depend on which tree happened to be visited first: every trunk
            // write produces Wood, every leaf write produces Leaves-into-Air, and
            // both operations are idempotent within their own sub-pass.
            std::array<TreeInst, 9 * kTreeSlots> trees{};
            int treeCount = 0;

            for (int ncz = chunk.key.cz - 1; ncz <= chunk.key.cz + 1; ++ncz) {
                for (int ncx = chunk.key.cx - 1; ncx <= chunk.key.cx + 1; ++ncx) {
                    for (int i = 0; i < kTreeSlots; ++i) {
                        const int wx = ncx * kCS +
                                       static_cast<int>(noise::rand01(seed + kSaltTree, ncx, ncz, i) * kCS);
                        const int wz = ncz * kCS +
                                       static_cast<int>(noise::rand01(seed + kSaltTree + 1, ncx, ncz, i) * kCS);
                        if (wx + kTreeReach < baseX || wx - kTreeReach >= baseX + kCS) continue;
                        if (wz + kTreeReach < baseZ || wz - kTreeReach >= baseZ + kCS) continue;

                        const float p = noise::rand01(seed + kSaltTree + 2, ncx, ncz, i);
                        if (p >= kTreeMaxP) continue; // cheap gate before sampling

                        const Column c = sampleColumn(seed, wx, wz);
                        if (p >= treeDensity(c.biome)) continue;
                        if (c.height <= sea) continue;                 // no trees in water
                        if (c.temp < 0.28f && c.height > 62) continue; // bare summits

                        TreeInst t;
                        t.wx = wx;
                        t.wz = wz;
                        t.ground = c.height;
                        t.kind = treeKindFor(c.biome, noise::rand01(seed + kSaltTree + 3, wx, wz, 0));
                        trees[static_cast<std::size_t>(treeCount++)] = t;
                    }
                }
            }
            for (int i = 0; i < treeCount; ++i)
                stampTrunk(chunk, baseX, baseZ, seed, trees[static_cast<std::size_t>(i)]);
            for (int i = 0; i < treeCount; ++i)
                stampCanopy(chunk, baseX, baseZ, seed, trees[static_cast<std::size_t>(i)]);

            // ---- pass 6: landmarks ----------------------------------------------
            // Last, and their writes are unconditional: a landmark always wins over
            // whatever decoration happened to be there.
            placeStructures(chunk, seed);
        }

    } // namespace worldgen
} // namespace vox
