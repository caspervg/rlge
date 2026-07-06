#include "vx_blocks.hpp"

#include <algorithm>
#include <cmath>

#include "vx_noise.hpp"

namespace vox {

    namespace {
        // Tile index layout inside the atlas (row-major).
        enum Tile : int {
            T_GrassTop = 0,
            T_GrassSide,
            T_Dirt,
            T_Stone,
            T_Cobble,
            T_Sand,
            T_SnowTop,
            T_SnowSide,
            T_WoodBark,   // 8
            T_WoodRings,
            T_Leaves,
            T_Planks,
            T_Glass,
            T_Water,
            T_Bedrock,
            T_CoalOre,
            T_IronOre,    // 16
            T_Glowstone,
        };

        constexpr BlockInfo kInfos[kBlockCount] = {
            // name        solid  opaque cutout hardness top          side         bottom       mapColor
            {"air",        false, false, false, 0.00f, 0,           0,           0,           {0, 0, 0, 0}},
            {"grass",      true,  true,  false, 0.35f, T_GrassTop,  T_GrassSide, T_Dirt,      {96, 160, 66, 255}},
            {"dirt",       true,  true,  false, 0.30f, T_Dirt,      T_Dirt,      T_Dirt,      {134, 96, 67, 255}},
            {"stone",      true,  true,  false, 0.90f, T_Stone,     T_Stone,     T_Stone,     {130, 130, 134, 255}},
            {"cobble",     true,  true,  false, 1.00f, T_Cobble,    T_Cobble,    T_Cobble,    {110, 110, 112, 255}},
            {"sand",       true,  true,  false, 0.30f, T_Sand,      T_Sand,      T_Sand,      {219, 207, 163, 255}},
            {"snow",       true,  true,  false, 0.35f, T_SnowTop,   T_SnowSide,  T_Dirt,      {236, 240, 245, 255}},
            {"wood",       true,  true,  false, 0.80f, T_WoodRings, T_WoodBark,  T_WoodRings, {112, 88, 55, 255}},
            {"leaves",     true,  false, true,  0.20f, T_Leaves,    T_Leaves,    T_Leaves,    {58, 122, 48, 255}},
            {"planks",     true,  true,  false, 0.70f, T_Planks,    T_Planks,    T_Planks,    {172, 138, 88, 255}},
            {"glass",      true,  false, true,  0.25f, T_Glass,     T_Glass,     T_Glass,     {200, 226, 235, 255}},
            {"water",      false, false, false, 0.00f, T_Water,     T_Water,     T_Water,     {46, 90, 200, 255}},
            {"bedrock",    true,  true,  false, -1.0f, T_Bedrock,   T_Bedrock,   T_Bedrock,   {40, 40, 44, 255}},
            {"coal ore",   true,  true,  false, 1.10f, T_CoalOre,   T_CoalOre,   T_CoalOre,   {70, 70, 72, 255}},
            {"iron ore",   true,  true,  false, 1.30f, T_IronOre,   T_IronOre,   T_IronOre,   {188, 152, 122, 255}},
            {"glowstone",  true,  true,  false, 0.40f, T_Glowstone, T_Glowstone, T_Glowstone, {255, 214, 110, 255}},
        };

        float n2(const std::uint32_t seed, const int px, const int py, const float scale) {
            return noise::value2(seed, static_cast<float>(px) * scale, static_cast<float>(py) * scale);
        }

        Color shade(const Color base, const float mul) {
            const auto clamp255 = [](const float v) {
                return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
            };
            return {clamp255(base.r * mul), clamp255(base.g * mul), clamp255(base.b * mul), base.a};
        }

        // Paint one 16x16 tile into the atlas image.
        void paintTile(Image& img, const int tile, const std::uint32_t seed,
                       const Color base, const float grainScale, const float grainAmount) {
            const int ox = (tile % kAtlasCols) * kTilePx;
            const int oy = (tile / kAtlasCols) * kTilePx;
            for (int y = 0; y < kTilePx; ++y) {
                for (int x = 0; x < kTilePx; ++x) {
                    const float g = n2(seed + static_cast<std::uint32_t>(tile) * 977U, x, y, grainScale);
                    const float mul = 1.0f - grainAmount * 0.5f + grainAmount * g;
                    ImageDrawPixel(&img, ox + x, oy + y, shade(base, mul));
                }
            }
        }

        void px(Image& img, const int tile, const int x, const int y, const Color c) {
            const int ox = (tile % kAtlasCols) * kTilePx;
            const int oy = (tile / kAtlasCols) * kTilePx;
            ImageDrawPixel(&img, ox + x, oy + y, c);
        }
    } // namespace

    const BlockInfo& blockInfo(const Block b) { return kInfos[static_cast<int>(b)]; }

    Rectangle tileUV(const int tile) {
        const float w = 1.0f / kAtlasCols;
        const float h = 1.0f / kAtlasRows;
        return Rectangle{static_cast<float>(tile % kAtlasCols) * w,
                         static_cast<float>(tile / kAtlasCols) * h, w, h};
    }

    Texture2D buildAtlas(const std::uint32_t seed) {
        Image img = GenImageColor(kAtlasCols * kTilePx, kAtlasRows * kTilePx, BLANK);

        const Color grassGreen{106, 176, 76, 255};
        const Color dirtBrown{134, 96, 67, 255};
        const Color stoneGray{128, 128, 132, 255};
        const Color sandTan{219, 207, 163, 255};
        const Color snowWhite{236, 240, 246, 255};
        const Color barkBrown{104, 82, 50, 255};
        const Color ringTan{168, 134, 84, 255};
        const Color leafGreen{62, 128, 50, 255};
        const Color plankTan{176, 142, 92, 255};
        const Color waterBlue{52, 104, 218, 235};
        const Color bedrockDark{44, 44, 48, 255};
        const Color glowYellow{255, 208, 96, 255};

        paintTile(img, T_GrassTop, seed, grassGreen, 0.9f, 0.35f);
        paintTile(img, T_Dirt, seed, dirtBrown, 0.8f, 0.4f);
        paintTile(img, T_Stone, seed, stoneGray, 0.5f, 0.3f);
        paintTile(img, T_Sand, seed, sandTan, 0.9f, 0.25f);
        paintTile(img, T_SnowTop, seed, snowWhite, 0.9f, 0.12f);
        paintTile(img, T_Bedrock, seed, bedrockDark, 0.7f, 0.55f);
        paintTile(img, T_Water, seed, waterBlue, 0.6f, 0.18f);
        paintTile(img, T_Glowstone, seed, glowYellow, 0.8f, 0.3f);

        // Grass side: dirt with a ragged green fringe on top.
        paintTile(img, T_GrassSide, seed + 1, dirtBrown, 0.8f, 0.4f);
        for (int x = 0; x < kTilePx; ++x) {
            const int fringe = 3 + static_cast<int>(noise::rand01(seed, x, 77) * 3.0f);
            for (int y = 0; y < fringe; ++y) {
                px(img, T_GrassSide, x, y, shade(grassGreen, 0.85f + 0.3f * noise::rand01(seed, x, y)));
            }
        }

        // Snow side: dirt with a snow cap.
        paintTile(img, T_SnowSide, seed + 2, dirtBrown, 0.8f, 0.4f);
        for (int x = 0; x < kTilePx; ++x) {
            const int cap = 3 + static_cast<int>(noise::rand01(seed, x, 91) * 2.0f);
            for (int y = 0; y < cap; ++y) {
                px(img, T_SnowSide, x, y, shade(snowWhite, 0.9f + 0.15f * noise::rand01(seed, x, y + 3)));
            }
        }

        // Cobble: stone with mortar cracks between blobs.
        paintTile(img, T_Cobble, seed + 3, stoneGray, 0.45f, 0.3f);
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                const float blob = noise::value2(seed + 55, x * 0.45f, y * 0.45f);
                if (blob > 0.44f && blob < 0.52f) {
                    px(img, T_Cobble, x, y, shade(stoneGray, 0.55f));
                }
            }
        }

        // Bark: vertical streaks.
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                const float streak = noise::value2(seed + 66, x * 1.6f, y * 0.25f);
                px(img, T_WoodBark, x, y, shade(barkBrown, 0.75f + 0.5f * streak));
            }
        }

        // Wood rings: concentric rings around the center.
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                const float dx = x - 7.5f;
                const float dy = y - 7.5f;
                const float r = std::sqrt(dx * dx + dy * dy);
                const float ring = 0.5f + 0.5f * std::sin(r * 1.9f);
                px(img, T_WoodRings, x, y, shade(ringTan, 0.7f + 0.35f * ring));
            }
        }

        // Leaves: clumpy green with alpha holes (cutout).
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                const float clump = noise::value2(seed + 77, x * 0.8f, y * 0.8f);
                Color c = shade(leafGreen, 0.6f + 0.7f * clump);
                if (noise::rand01(seed + 78, x, y) > 0.86f) {
                    c.a = 0; // small holes let the sky peek through
                }
                px(img, T_Leaves, x, y, c);
            }
        }

        // Planks: boards with seams.
        paintTile(img, T_Planks, seed + 4, plankTan, 1.2f, 0.22f);
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                if (y % 4 == 3) {
                    px(img, T_Planks, x, y, shade(plankTan, 0.6f));
                } else if ((x + (y / 4) * 8) % 16 == 0) {
                    px(img, T_Planks, x, y, shade(plankTan, 0.65f));
                }
            }
        }

        // Glass: transparent pane with a bright frame and a few glints.
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                const bool frame = x == 0 || y == 0 || x == kTilePx - 1 || y == kTilePx - 1;
                if (frame) {
                    px(img, T_Glass, x, y, Color{225, 240, 248, 255});
                } else if ((x + y) % 7 == 0 && x > 2 && y > 2 && x < 13) {
                    px(img, T_Glass, x, y, Color{235, 246, 252, 120});
                } else {
                    px(img, T_Glass, x, y, Color{210, 235, 245, 0});
                }
            }
        }

        // Ores: stone with mineral flecks.
        paintTile(img, T_CoalOre, seed + 5, stoneGray, 0.5f, 0.3f);
        paintTile(img, T_IronOre, seed + 6, stoneGray, 0.5f, 0.3f);
        for (int i = 0; i < 7; ++i) {
            const int fx = 1 + static_cast<int>(noise::rand01(seed + 7, i, 0) * 13.0f);
            const int fy = 1 + static_cast<int>(noise::rand01(seed + 7, i, 1) * 13.0f);
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    px(img, T_CoalOre, fx + dx, fy + dy, Color{34, 34, 38, 255});
                    px(img, T_IronOre, (fx + 5 + dx) % 15, (fy + 3 + dy) % 15, Color{216, 168, 128, 255});
                }
            }
        }

        // Glowstone: brighter crystal veins on the warm base.
        for (int y = 0; y < kTilePx; ++y) {
            for (int x = 0; x < kTilePx; ++x) {
                if (noise::value2(seed + 88, x * 0.7f, y * 0.7f) > 0.62f) {
                    px(img, T_Glowstone, x, y, Color{255, 244, 190, 255});
                }
            }
        }

        Texture2D tex = LoadTextureFromImage(img);
        SetTextureFilter(tex, TEXTURE_FILTER_POINT); // crisp voxel pixels
        SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
        UnloadImage(img);
        return tex;
    }

} // namespace vox
