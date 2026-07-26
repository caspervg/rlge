#include "vx_mobs.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "rlgl.h"

#include "vx_config.hpp"
#include "vx_noise.hpp"

namespace vox {

    namespace {
        // ------------------------------------------------------------- atlas
        // Mob skins live in their own 8x8 grid of 16x16 tiles, painted with the
        // same value-noise grain as the terrain so creatures sit in the same
        // visual world rather than looking like imported art.
        constexpr int kTilePx = 16;
        constexpr int kCols = 8;
        constexpr int kRows = 8;

        enum Tile : int {
            T_PigSkin = 0, T_PigFace, T_PigSnout,
            T_Wool, T_SheepFace, T_SheepSkin,
            T_CowHide, T_CowFace,
            T_Feather, T_ChickenFace, T_Beak,          // 8,9,10
            T_ZombieSkin, T_ZombieFace, T_ZombieShirt,
            T_Bone, T_SkeletonFace,                     // 14,15
            T_CreeperSkin, T_CreeperFace,
            T_SpiderBody, T_SpiderFace, T_SpiderLeg,    // 18,19,20
        };

        Rectangle tileRect(const int tile) {
            const float w = 1.0f / kCols;
            const float h = 1.0f / kRows;
            return {static_cast<float>(tile % kCols) * w,
                    static_cast<float>(tile / kCols) * h, w, h};
        }

        Color shadeCol(const Color base, const float mul) {
            const auto ch = [](const float v) {
                return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
            };
            return {ch(base.r * mul), ch(base.g * mul), ch(base.b * mul), base.a};
        }

        void px(Image& img, const int tile, const int x, const int y, const Color c) {
            ImageDrawPixel(&img, (tile % kCols) * kTilePx + x, (tile / kCols) * kTilePx + y, c);
        }

        // Flat colour plus fine grain, matching the terrain painter's look.
        void grain(Image& img, const int tile, const std::uint32_t seed, const Color base,
                   const float scale, const float amount) {
            for (int y = 0; y < kTilePx; ++y) {
                for (int x = 0; x < kTilePx; ++x) {
                    const float n = noise::value2(seed + static_cast<std::uint32_t>(tile) * 733U,
                                                  static_cast<float>(x) * scale,
                                                  static_cast<float>(y) * scale);
                    px(img, tile, x, y, shadeCol(base, 1.0f - amount * 0.5f + amount * n));
                }
            }
        }

        // Rectangle of pixels, used for eyes, mouths and markings.
        void rect(Image& img, const int tile, const int x0, const int y0, const int w, const int h,
                  const Color c) {
            for (int y = y0; y < y0 + h; ++y)
                for (int x = x0; x < x0 + w; ++x)
                    if (x >= 0 && x < kTilePx && y >= 0 && y < kTilePx)
                        px(img, tile, x, y, c);
        }

        Texture2D buildMobAtlas(const std::uint32_t seed) {
            Image img = GenImageColor(kCols * kTilePx, kRows * kTilePx, BLANK);

            const Color pink{240, 155, 160, 255};
            const Color wool{233, 233, 228, 255};
            const Color sheepSkin{224, 190, 170, 255};
            const Color cowBrown{74, 54, 40, 255};
            const Color feather{238, 238, 238, 255};
            const Color zombieGreen{92, 148, 82, 255};
            const Color zombieShirt{60, 92, 148, 255};
            const Color bone{224, 224, 216, 255};
            const Color creeperGreen{104, 190, 88, 255};
            const Color spiderDark{48, 40, 40, 255};
            const Color eyeBlack{28, 24, 24, 255};
            const Color eyeWhite{240, 240, 240, 255};
            const Color eyeRed{214, 48, 48, 255};

            grain(img, T_PigSkin, seed, pink, 0.9f, 0.18f);
            grain(img, T_PigSnout, seed, shadeCol(pink, 0.88f), 0.9f, 0.16f);
            grain(img, T_Wool, seed, wool, 1.6f, 0.34f);       // fleecy, high contrast
            grain(img, T_SheepSkin, seed, sheepSkin, 0.8f, 0.16f);
            grain(img, T_Feather, seed, feather, 1.1f, 0.14f);
            grain(img, T_Beak, seed, Color{226, 160, 62, 255}, 0.8f, 0.16f);
            grain(img, T_ZombieSkin, seed, zombieGreen, 0.9f, 0.24f);
            grain(img, T_ZombieShirt, seed, zombieShirt, 0.9f, 0.20f);
            grain(img, T_Bone, seed, bone, 1.0f, 0.18f);
            grain(img, T_CreeperSkin, seed, creeperGreen, 1.5f, 0.36f); // mottled
            grain(img, T_SpiderBody, seed, spiderDark, 1.2f, 0.30f);
            grain(img, T_SpiderLeg, seed, shadeCol(spiderDark, 0.8f), 1.0f, 0.24f);

            // Cow hide: brown with irregular white patches.
            grain(img, T_CowHide, seed, cowBrown, 0.9f, 0.22f);
            for (int y = 0; y < kTilePx; ++y)
                for (int x = 0; x < kTilePx; ++x)
                    if (noise::value2(seed + 91, x * 0.42f, y * 0.42f) > 0.60f)
                        px(img, T_CowHide, x, y, shadeCol(Color{225, 222, 215, 255},
                                                          0.9f + 0.2f * noise::rand01(seed, x, y)));

            // --- faces -------------------------------------------------------
            // Pig: snout plate with two nostrils, eyes to the sides.
            grain(img, T_PigFace, seed, pink, 0.9f, 0.18f);
            rect(img, T_PigFace, 5, 7, 6, 6, shadeCol(pink, 0.86f));
            rect(img, T_PigFace, 6, 9, 2, 2, eyeBlack);
            rect(img, T_PigFace, 9, 9, 2, 2, eyeBlack);
            rect(img, T_PigFace, 2, 3, 3, 3, eyeWhite);
            rect(img, T_PigFace, 3, 4, 2, 2, eyeBlack);
            rect(img, T_PigFace, 11, 3, 3, 3, eyeWhite);
            rect(img, T_PigFace, 11, 4, 2, 2, eyeBlack);

            // Sheep: woolly brow over a bare face.
            grain(img, T_SheepFace, seed, sheepSkin, 0.8f, 0.16f);
            rect(img, T_SheepFace, 0, 0, kTilePx, 5, shadeCol(wool, 1.0f));
            rect(img, T_SheepFace, 3, 7, 3, 3, eyeBlack);
            rect(img, T_SheepFace, 10, 7, 3, 3, eyeBlack);

            // Cow: white muzzle band, dark eyes.
            grain(img, T_CowFace, seed, cowBrown, 0.9f, 0.22f);
            rect(img, T_CowFace, 3, 9, 10, 5, Color{222, 218, 210, 255});
            rect(img, T_CowFace, 5, 11, 2, 2, shadeCol(cowBrown, 0.7f));
            rect(img, T_CowFace, 9, 11, 2, 2, shadeCol(cowBrown, 0.7f));
            rect(img, T_CowFace, 2, 3, 3, 3, eyeWhite);
            rect(img, T_CowFace, 3, 4, 2, 2, eyeBlack);
            rect(img, T_CowFace, 11, 3, 3, 3, eyeWhite);
            rect(img, T_CowFace, 11, 4, 2, 2, eyeBlack);

            // Chicken: beady eyes and a wattle.
            grain(img, T_ChickenFace, seed, feather, 1.1f, 0.14f);
            rect(img, T_ChickenFace, 3, 5, 3, 3, eyeBlack);
            rect(img, T_ChickenFace, 10, 5, 3, 3, eyeBlack);
            rect(img, T_ChickenFace, 6, 11, 4, 4, Color{198, 60, 52, 255});

            // Zombie: sunken black eyes and a grim mouth.
            grain(img, T_ZombieFace, seed, zombieGreen, 0.9f, 0.24f);
            rect(img, T_ZombieFace, 2, 6, 4, 3, eyeBlack);
            rect(img, T_ZombieFace, 10, 6, 4, 3, eyeBlack);
            rect(img, T_ZombieFace, 5, 12, 6, 2, shadeCol(zombieGreen, 0.55f));

            // Skeleton: hollow sockets and a toothed jaw.
            grain(img, T_SkeletonFace, seed, bone, 1.0f, 0.18f);
            rect(img, T_SkeletonFace, 2, 5, 4, 4, eyeBlack);
            rect(img, T_SkeletonFace, 10, 5, 4, 4, eyeBlack);
            rect(img, T_SkeletonFace, 4, 11, 8, 3, shadeCol(bone, 0.5f));
            for (int i = 0; i < 4; ++i)
                rect(img, T_SkeletonFace, 5 + i * 2, 11, 1, 3, bone);

            // Creeper: the four-square face.
            grain(img, T_CreeperFace, seed, creeperGreen, 1.5f, 0.36f);
            rect(img, T_CreeperFace, 3, 4, 3, 3, eyeBlack);
            rect(img, T_CreeperFace, 10, 4, 3, 3, eyeBlack);
            rect(img, T_CreeperFace, 6, 8, 4, 3, eyeBlack);
            rect(img, T_CreeperFace, 5, 10, 2, 4, eyeBlack);
            rect(img, T_CreeperFace, 9, 10, 2, 4, eyeBlack);

            // Spider: a cluster of red eyes.
            grain(img, T_SpiderFace, seed, spiderDark, 1.2f, 0.30f);
            rect(img, T_SpiderFace, 3, 5, 3, 3, eyeRed);
            rect(img, T_SpiderFace, 10, 5, 3, 3, eyeRed);
            rect(img, T_SpiderFace, 6, 9, 2, 2, eyeRed);
            rect(img, T_SpiderFace, 8, 9, 2, 2, eyeRed);

            Texture2D tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_POINT);
            UnloadImage(img);
            return tex;
        }

        // -------------------------------------------------------------- model

        enum class Swing : std::uint8_t {
            None,
            LegA,   // front-left / back-right pair
            LegB,   // the opposite pair
            ArmA,
            ArmB,
            Head,
        };

        struct Part {
            Vector3 off;     // centre offset from feet centre; +X is forward
            Vector3 size;
            int tile;
            int faceTile;    // drawn on the +X face when >= 0
            Swing swing;
        };

        // Limb pivots sit at the top of the part so legs hinge at the hip.
        struct Model {
            const Part* parts;
            int count;
        };

        constexpr Part kPig[] = {
            {{0.00f, 0.62f, 0.00f}, {0.95f, 0.45f, 0.60f}, T_PigSkin, -1, Swing::None},
            {{0.62f, 0.70f, 0.00f}, {0.50f, 0.45f, 0.50f}, T_PigSkin, T_PigFace, Swing::Head},
            {{0.90f, 0.64f, 0.00f}, {0.16f, 0.18f, 0.26f}, T_PigSnout, T_PigSnout, Swing::Head},
            {{ 0.30f, 0.20f,  0.18f}, {0.20f, 0.40f, 0.20f}, T_PigSkin, -1, Swing::LegA},
            {{ 0.30f, 0.20f, -0.18f}, {0.20f, 0.40f, 0.20f}, T_PigSkin, -1, Swing::LegB},
            {{-0.30f, 0.20f,  0.18f}, {0.20f, 0.40f, 0.20f}, T_PigSkin, -1, Swing::LegB},
            {{-0.30f, 0.20f, -0.18f}, {0.20f, 0.40f, 0.20f}, T_PigSkin, -1, Swing::LegA},
        };

        constexpr Part kSheep[] = {
            {{0.00f, 0.74f, 0.00f}, {1.00f, 0.60f, 0.70f}, T_Wool, -1, Swing::None},
            {{0.64f, 0.86f, 0.00f}, {0.42f, 0.42f, 0.42f}, T_SheepSkin, T_SheepFace, Swing::Head},
            {{ 0.28f, 0.22f,  0.20f}, {0.18f, 0.44f, 0.18f}, T_SheepSkin, -1, Swing::LegA},
            {{ 0.28f, 0.22f, -0.20f}, {0.18f, 0.44f, 0.18f}, T_SheepSkin, -1, Swing::LegB},
            {{-0.28f, 0.22f,  0.20f}, {0.18f, 0.44f, 0.18f}, T_SheepSkin, -1, Swing::LegB},
            {{-0.28f, 0.22f, -0.20f}, {0.18f, 0.44f, 0.18f}, T_SheepSkin, -1, Swing::LegA},
        };

        constexpr Part kCow[] = {
            {{0.00f, 0.88f, 0.00f}, {1.10f, 0.60f, 0.70f}, T_CowHide, -1, Swing::None},
            {{0.74f, 1.00f, 0.00f}, {0.46f, 0.46f, 0.46f}, T_CowHide, T_CowFace, Swing::Head},
            {{0.72f, 1.26f,  0.26f}, {0.12f, 0.14f, 0.12f}, T_Bone, -1, Swing::Head},
            {{0.72f, 1.26f, -0.26f}, {0.12f, 0.14f, 0.12f}, T_Bone, -1, Swing::Head},
            {{ 0.32f, 0.29f,  0.22f}, {0.20f, 0.58f, 0.20f}, T_CowHide, -1, Swing::LegA},
            {{ 0.32f, 0.29f, -0.22f}, {0.20f, 0.58f, 0.20f}, T_CowHide, -1, Swing::LegB},
            {{-0.32f, 0.29f,  0.22f}, {0.20f, 0.58f, 0.20f}, T_CowHide, -1, Swing::LegB},
            {{-0.32f, 0.29f, -0.22f}, {0.20f, 0.58f, 0.20f}, T_CowHide, -1, Swing::LegA},
        };

        constexpr Part kChicken[] = {
            {{0.00f, 0.42f, 0.00f}, {0.42f, 0.36f, 0.32f}, T_Feather, -1, Swing::None},
            {{0.22f, 0.66f, 0.00f}, {0.26f, 0.26f, 0.26f}, T_Feather, T_ChickenFace, Swing::Head},
            {{0.40f, 0.62f, 0.00f}, {0.14f, 0.08f, 0.10f}, T_Beak, T_Beak, Swing::Head},
            {{0.02f, 0.44f,  0.19f}, {0.30f, 0.26f, 0.05f}, T_Feather, -1, Swing::ArmA},
            {{0.02f, 0.44f, -0.19f}, {0.30f, 0.26f, 0.05f}, T_Feather, -1, Swing::ArmB},
            {{0.00f, 0.12f,  0.09f}, {0.10f, 0.24f, 0.10f}, T_Beak, -1, Swing::LegA},
            {{0.00f, 0.12f, -0.09f}, {0.10f, 0.24f, 0.10f}, T_Beak, -1, Swing::LegB},
        };

        constexpr Part kZombie[] = {
            {{0.00f, 1.10f, 0.00f}, {0.26f, 0.72f, 0.52f}, T_ZombieShirt, -1, Swing::None},
            {{0.00f, 1.71f, 0.00f}, {0.50f, 0.50f, 0.50f}, T_ZombieSkin, T_ZombieFace, Swing::Head},
            // Arms held straight out in front - the pose reads as "zombie" instantly.
            {{0.34f, 1.32f,  0.38f}, {0.62f, 0.22f, 0.22f}, T_ZombieSkin, -1, Swing::ArmA},
            {{0.34f, 1.32f, -0.38f}, {0.62f, 0.22f, 0.22f}, T_ZombieSkin, -1, Swing::ArmB},
            {{0.00f, 0.37f,  0.14f}, {0.24f, 0.74f, 0.24f}, T_ZombieShirt, -1, Swing::LegA},
            {{0.00f, 0.37f, -0.14f}, {0.24f, 0.74f, 0.24f}, T_ZombieShirt, -1, Swing::LegB},
        };

        constexpr Part kSkeleton[] = {
            {{0.00f, 1.08f, 0.00f}, {0.20f, 0.70f, 0.44f}, T_Bone, -1, Swing::None},
            {{0.00f, 1.68f, 0.00f}, {0.50f, 0.50f, 0.50f}, T_Bone, T_SkeletonFace, Swing::Head},
            {{0.10f, 1.30f,  0.32f}, {0.16f, 0.68f, 0.16f}, T_Bone, -1, Swing::ArmA},
            {{0.10f, 1.30f, -0.32f}, {0.16f, 0.68f, 0.16f}, T_Bone, -1, Swing::ArmB},
            {{0.00f, 0.36f,  0.12f}, {0.16f, 0.72f, 0.16f}, T_Bone, -1, Swing::LegA},
            {{0.00f, 0.36f, -0.12f}, {0.16f, 0.72f, 0.16f}, T_Bone, -1, Swing::LegB},
        };

        constexpr Part kCreeper[] = {
            {{0.00f, 0.96f, 0.00f}, {0.30f, 0.78f, 0.50f}, T_CreeperSkin, -1, Swing::None},
            {{0.00f, 1.58f, 0.00f}, {0.50f, 0.50f, 0.50f}, T_CreeperSkin, T_CreeperFace, Swing::Head},
            {{ 0.18f, 0.19f,  0.16f}, {0.22f, 0.38f, 0.22f}, T_CreeperSkin, -1, Swing::LegA},
            {{ 0.18f, 0.19f, -0.16f}, {0.22f, 0.38f, 0.22f}, T_CreeperSkin, -1, Swing::LegB},
            {{-0.18f, 0.19f,  0.16f}, {0.22f, 0.38f, 0.22f}, T_CreeperSkin, -1, Swing::LegB},
            {{-0.18f, 0.19f, -0.16f}, {0.22f, 0.38f, 0.22f}, T_CreeperSkin, -1, Swing::LegA},
        };

        constexpr Part kSpider[] = {
            {{-0.26f, 0.50f, 0.00f}, {0.66f, 0.46f, 0.66f}, T_SpiderBody, -1, Swing::None},
            {{ 0.32f, 0.46f, 0.00f}, {0.44f, 0.36f, 0.46f}, T_SpiderBody, T_SpiderFace, Swing::None},
            // Four legs a side, splayed out and stepping in two alternating sets.
            {{ 0.20f, 0.30f,  0.44f}, {0.16f, 0.14f, 0.52f}, T_SpiderLeg, -1, Swing::LegA},
            {{ 0.02f, 0.30f,  0.46f}, {0.16f, 0.14f, 0.56f}, T_SpiderLeg, -1, Swing::LegB},
            {{-0.16f, 0.30f,  0.46f}, {0.16f, 0.14f, 0.56f}, T_SpiderLeg, -1, Swing::LegA},
            {{-0.34f, 0.30f,  0.44f}, {0.16f, 0.14f, 0.52f}, T_SpiderLeg, -1, Swing::LegB},
            {{ 0.20f, 0.30f, -0.44f}, {0.16f, 0.14f, 0.52f}, T_SpiderLeg, -1, Swing::LegB},
            {{ 0.02f, 0.30f, -0.46f}, {0.16f, 0.14f, 0.56f}, T_SpiderLeg, -1, Swing::LegA},
            {{-0.16f, 0.30f, -0.46f}, {0.16f, 0.14f, 0.56f}, T_SpiderLeg, -1, Swing::LegB},
            {{-0.34f, 0.30f, -0.44f}, {0.16f, 0.14f, 0.52f}, T_SpiderLeg, -1, Swing::LegA},
        };

        Model modelFor(const MobKind k) {
            switch (k) {
            case MobKind::Pig: return {kPig, static_cast<int>(std::size(kPig))};
            case MobKind::Sheep: return {kSheep, static_cast<int>(std::size(kSheep))};
            case MobKind::Cow: return {kCow, static_cast<int>(std::size(kCow))};
            case MobKind::Chicken: return {kChicken, static_cast<int>(std::size(kChicken))};
            case MobKind::Zombie: return {kZombie, static_cast<int>(std::size(kZombie))};
            case MobKind::Skeleton: return {kSkeleton, static_cast<int>(std::size(kSkeleton))};
            case MobKind::Creeper: return {kCreeper, static_cast<int>(std::size(kCreeper))};
            case MobKind::Spider: return {kSpider, static_cast<int>(std::size(kSpider))};
            default: return {kPig, static_cast<int>(std::size(kPig))};
            }
        }

        constexpr MobStats kStats[kMobKindCount] = {
            //  name        hostile  w      h     speed  hp  dmg  atk   sense  burns
            {"pig",       false, 0.90f, 0.90f, 1.6f, 10, 0, 0.0f,  0.0f, false},
            {"sheep",     false, 0.90f, 1.05f, 1.5f, 8,  0, 0.0f,  0.0f, false},
            {"cow",       false, 0.90f, 1.35f, 1.4f, 10, 0, 0.0f,  0.0f, false},
            {"chicken",   false, 0.40f, 0.70f, 1.7f, 4,  0, 0.0f,  0.0f, false},
            {"zombie",    true,  0.60f, 1.95f, 2.4f, 20, 3, 1.5f, 22.0f, true},
            {"skeleton",  true,  0.60f, 1.90f, 2.6f, 16, 2, 1.6f, 24.0f, true},
            {"creeper",   true,  0.60f, 1.80f, 2.5f, 16, 6, 1.4f, 20.0f, false},
            {"spider",    true,  1.10f, 0.75f, 3.1f, 14, 2, 1.5f, 18.0f, false},
        };

        float frand(const std::uint32_t& s) {
            return static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
        }

        // Per-face shade, matching the terrain's directional bake.
        constexpr float kFaceShade[6] = {1.0f, 0.5f, 0.72f, 0.72f, 0.86f, 0.86f};

        // Emits one textured box in the current rlgl matrix. Faces are ordered
        // +Y, -Y, +X, -X, +Z, -Z so kFaceShade lines up.
        void boxFaces(const Vector3 c, const Vector3 s, const Rectangle skin, const Rectangle face,
                      const Color tint) {
            const float x0 = c.x - s.x * 0.5f, x1 = c.x + s.x * 0.5f;
            const float y0 = c.y - s.y * 0.5f, y1 = c.y + s.y * 0.5f;
            const float z0 = c.z - s.z * 0.5f, z1 = c.z + s.z * 0.5f;

            const auto emit = [&](const int f, const Vector3 a, const Vector3 b, const Vector3 d,
                                  const Vector3 e) {
                const Rectangle uv = (f == 2 && face.width > 0.0f) ? face : skin;
                const Color col = shadeCol(tint, kFaceShade[f]);
                rlColor4ub(col.r, col.g, col.b, col.a);
                // Inset by a texel to keep neighbouring atlas tiles from bleeding.
                const float ix = uv.width * 0.02f, iy = uv.height * 0.02f;
                rlTexCoord2f(uv.x + ix, uv.y + uv.height - iy);          rlVertex3f(a.x, a.y, a.z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + uv.height - iy); rlVertex3f(b.x, b.y, b.z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + iy);            rlVertex3f(d.x, d.y, d.z);
                rlTexCoord2f(uv.x + ix, uv.y + iy);                       rlVertex3f(e.x, e.y, e.z);
            };

            emit(0, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}); // +Y
            emit(1, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}); // -Y
            emit(2, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}); // +X (front)
            emit(3, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}); // -X
            emit(4, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}); // +Z
            emit(5, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}); // -Z
        }
    } // namespace

    const MobStats& mobStats(const MobKind kind) { return kStats[static_cast<int>(kind)]; }

    Color mobColor(const MobKind kind) {
        switch (kind) {
        case MobKind::Pig: return {240, 155, 160, 255};
        case MobKind::Sheep: return {233, 233, 228, 255};
        case MobKind::Cow: return {96, 72, 54, 255};
        case MobKind::Chicken: return {238, 238, 238, 255};
        case MobKind::Zombie: return {92, 148, 82, 255};
        case MobKind::Skeleton: return {224, 224, 216, 255};
        case MobKind::Creeper: return {104, 190, 88, 255};
        case MobKind::Spider: return {60, 50, 50, 255};
        default: return WHITE;
        }
    }

    Vector3 Mob::centre() const {
        return {pos.x, pos.y + mobStats(kind).height * 0.5f, pos.z};
    }

    BoundingBox Mob::bounds() const {
        const MobStats& st = mobStats(kind);
        const float hw = st.width * 0.5f;
        return {{pos.x - hw, pos.y, pos.z - hw},
                {pos.x + hw, pos.y + st.height, pos.z + hw}};
    }

    // ------------------------------------------------------------- lifecycle

    void MobManager::init(const std::uint32_t seed) {
        seed_ = seed;
        atlas_ = buildMobAtlas(seed);
        mobs_.reserve(64);
    }

    void MobManager::shutdown() {
        if (atlas_.id != 0) {
            UnloadTexture(atlas_);
            atlas_ = Texture2D{};
        }
        mobs_.clear();
    }

    int MobManager::passiveCount() const {
        return static_cast<int>(std::count_if(mobs_.begin(), mobs_.end(), [](const Mob& m) {
            return !m.dead && !mobStats(m.kind).hostile;
        }));
    }

    int MobManager::hostileCount() const {
        return static_cast<int>(std::count_if(mobs_.begin(), mobs_.end(), [](const Mob& m) {
            return !m.dead && mobStats(m.kind).hostile;
        }));
    }

    // ------------------------------------------------------------- physics

    bool MobManager::collides_(const World& world, const Mob& mob, const Vector3 at) const {
        const MobStats& st = mobStats(mob.kind);
        const float hw = st.width * 0.5f;
        const int x0 = static_cast<int>(std::floor(at.x - hw));
        const int x1 = static_cast<int>(std::floor(at.x + hw));
        const int y0 = static_cast<int>(std::floor(at.y));
        const int y1 = static_cast<int>(std::floor(at.y + st.height));
        const int z0 = static_cast<int>(std::floor(at.z - hw));
        const int z1 = static_cast<int>(std::floor(at.z + hw));
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    if (world.solidAt(x, y, z))
                        return true;
        return false;
    }

    void MobManager::moveAxis_(const World& world, Mob& mob, const int axis, const float amount) const {
        if (amount == 0.0f)
            return;
        const auto shift = [axis](Vector3 p, const float d) {
            if (axis == 0) p.x += d;
            else if (axis == 1) p.y += d;
            else p.z += d;
            return p;
        };
        if (!collides_(world, mob, shift(mob.pos, amount))) {
            mob.pos = shift(mob.pos, amount);
            return;
        }
        // Binary-search up to the obstruction so mobs sit flush against walls.
        float lo = 0.0f, hi = amount;
        for (int i = 0; i < 6; ++i) {
            const float mid = (lo + hi) * 0.5f;
            if (collides_(world, mob, shift(mob.pos, mid))) hi = mid; else lo = mid;
        }
        mob.pos = shift(mob.pos, lo);
        if (axis == 0) mob.vel.x = 0.0f;
        else if (axis == 1) mob.vel.y = 0.0f;
        else mob.vel.z = 0.0f;
    }

    // ------------------------------------------------------------ behaviour

    void MobManager::simulate_(World& world, Mob& mob, const Vector3 playerPos,
                               const float dayFactor, const float dt) {
        const MobStats& st = mobStats(mob.kind);

        if (mob.dead) {
            mob.deathTimer += dt;
            return;
        }

        mob.hurtFlash = std::max(0.0f, mob.hurtFlash - dt * 3.0f);
        mob.burning = std::max(0.0f, mob.burning - dt);
        mob.attackCooldown = std::max(0.0f, mob.attackCooldown - dt);
        mob.knockback = std::max(0.0f, mob.knockback - dt * 4.0f);

        const Vector3 toPlayer = Vector3Subtract(playerPos, mob.pos);
        const float dist = Vector3Length(toPlayer);

        // Undead scorch in daylight, which is what pushes them into caves and
        // makes nightfall feel different from noon.
        if (st.burnsInSun && dayFactor > 0.75f) {
            const int hx = static_cast<int>(std::floor(mob.pos.x));
            const int hz = static_cast<int>(std::floor(mob.pos.z));
            const int hy = static_cast<int>(std::floor(mob.pos.y + st.height));
            if (world.skyLight(hx, hy, hz) > 11) {
                mob.burning = 0.4f; // refreshed every frame while exposed
                mob.burnTimer += dt;
                if (mob.burnTimer > 0.5f) {
                    mob.burnTimer = 0.0f;
                    mob.health -= 1.0f;
                    if (mob.health <= 0.0f) {
                        mob.dead = true;
                        deaths.push_back({mob.centre(), mob.kind});
                        return;
                    }
                }
            }
        }

        // --- decide where to go -------------------------------------------
        bool chasing = false;
        if (st.hostile && dist < st.senseRange && dist > 0.01f) {
            chasing = true;
            mob.targetYaw = std::atan2(toPlayer.z, toPlayer.x);
            mob.moving = dist > st.attackRange * 0.8f;

            if (dist < st.attackRange && mob.attackCooldown <= 0.0f) {
                playerDamage_ += st.damage;
                lastHitDir_ = Vector3Normalize({toPlayer.x, 0.0f, toPlayer.z});
                mob.attackCooldown = 1.1f;
            }
        } else {
            // Idle wander: pick a new heading every few seconds.
            mob.wanderTimer -= dt;
            if (mob.wanderTimer <= 0.0f) {
                mob.wanderTimer = 2.0f + frand(seed_) * 4.0f;
                mob.moving = frand(seed_) > 0.35f;
                mob.targetYaw = frand(seed_) * 2.0f * PI;
            }
        }

        // Turn smoothly toward the target heading via the shortest arc.
        float delta = mob.targetYaw - mob.yaw;
        while (delta > PI) delta -= 2.0f * PI;
        while (delta < -PI) delta += 2.0f * PI;
        mob.yaw += delta * std::min(1.0f, dt * (chasing ? 8.0f : 3.0f));

        // --- move ----------------------------------------------------------
        const float speed = st.speed * (chasing ? 1.0f : 0.55f);
        Vector3 wish{0.0f, 0.0f, 0.0f};
        if (mob.moving) {
            wish = {std::cos(mob.yaw) * speed, 0.0f, std::sin(mob.yaw) * speed};
        }
        const float blend = std::min(1.0f, dt * 8.0f);
        mob.vel.x += (wish.x - mob.vel.x) * blend;
        mob.vel.z += (wish.z - mob.vel.z) * blend;
        mob.vel.y -= 26.0f * dt;
        mob.vel.y = std::max(mob.vel.y, -40.0f);

        const Vector3 before = mob.pos;
        moveAxis_(world, mob, 0, mob.vel.x * dt);
        moveAxis_(world, mob, 2, mob.vel.z * dt);

        // Blocked while trying to walk? Hop, the same way Minecraft mobs climb
        // a one-block step instead of pathfinding around it.
        const float travelled = std::fabs(mob.pos.x - before.x) + std::fabs(mob.pos.z - before.z);
        const float wanted = (std::fabs(mob.vel.x) + std::fabs(mob.vel.z)) * dt;
        if (mob.moving && mob.onGround && wanted > 0.001f && travelled < wanted * 0.4f) {
            mob.vel.y = 7.4f;
        }

        const float fallSpeed = mob.vel.y;
        moveAxis_(world, mob, 1, mob.vel.y * dt);
        mob.onGround = fallSpeed < 0.0f && mob.vel.y == 0.0f;

        // Drowning-free: bob up in water rather than sinking forever.
        const int wx = static_cast<int>(std::floor(mob.pos.x));
        const int wy = static_cast<int>(std::floor(mob.pos.y + 0.4f));
        const int wz = static_cast<int>(std::floor(mob.pos.z));
        if (world.block(wx, wy, wz) == Block::Water) {
            mob.vel.y = std::min(mob.vel.y + 34.0f * dt, 2.6f);
        }

        // Anything that falls out of the world is gone.
        if (mob.pos.y < -4.0f) {
            mob.dead = true;
            mob.deathTimer = 1.0f;
        }

        // --- animation ------------------------------------------------------
        const float planar = std::sqrt(mob.vel.x * mob.vel.x + mob.vel.z * mob.vel.z);
        const float targetSwing = std::min(planar / std::max(st.speed, 0.01f), 1.0f);
        mob.limbSwing += (targetSwing - mob.limbSwing) * std::min(1.0f, dt * 8.0f);
        mob.limbPhase += planar * dt * 2.6f;
    }

    // -------------------------------------------------------------- spawning

    void MobManager::trySpawn_(World& world, const Vector3 playerPos, const float dayFactor) {
        const bool night = dayFactor < 0.35f;
        const int passives = passiveCount();
        const int hostiles = hostileCount();

        const bool wantPassive = passives < 12;
        const bool wantHostile = night && hostiles < 14;
        if (!wantPassive && !wantHostile)
            return;

        // Prefer whichever quota is emptier so neither starves.
        const bool spawnHostile = wantHostile && (!wantPassive || frand(seed_) < 0.6f);

        for (int attempt = 0; attempt < 8; ++attempt) {
            const float angle = frand(seed_) * 2.0f * PI;
            const float radius = 14.0f + frand(seed_) * 26.0f;
            const int x = static_cast<int>(std::floor(playerPos.x + std::cos(angle) * radius));
            const int z = static_cast<int>(std::floor(playerPos.z + std::sin(angle) * radius));

            // Only spawn where the world is actually loaded.
            if (world.chunkAt(x, z) == nullptr)
                continue;

            const int ground = world.surfaceHeight(x, z);
            const int y = ground + 1;
            if (y < 2 || y >= cfg.worldHeight - 3)
                continue;
            const Block below = world.block(x, ground, z);
            if (below == Block::Water || below == Block::Air)
                continue;
            if (world.solidAt(x, y, z) || world.solidAt(x, y + 1, z))
                continue;

            const int sky = world.skyLight(x, y, z);
            MobKind kind;
            if (spawnHostile) {
                if (sky > 7) // hostiles need darkness even at night
                    continue;
                const int roll = GetRandomValue(0, 99);
                kind = roll < 34 ? MobKind::Zombie
                     : roll < 62 ? MobKind::Skeleton
                     : roll < 82 ? MobKind::Spider
                                 : MobKind::Creeper;
            } else {
                if (sky < 9) // animals want open sky
                    continue;
                if (below != Block::Grass && below != Block::Snow && below != Block::Sand)
                    continue;
                const int roll = GetRandomValue(0, 99);
                kind = roll < 30 ? MobKind::Pig
                     : roll < 58 ? MobKind::Sheep
                     : roll < 80 ? MobKind::Cow
                                 : MobKind::Chicken;
            }

            Mob mob;
            mob.kind = kind;
            mob.pos = {static_cast<float>(x) + 0.5f, static_cast<float>(y),
                       static_cast<float>(z) + 0.5f};
            mob.health = static_cast<float>(mobStats(kind).maxHealth);
            mob.yaw = frand(seed_) * 2.0f * PI;
            mob.targetYaw = mob.yaw;
            mob.wanderTimer = frand(seed_) * 3.0f;
            mobs_.push_back(mob);
            return;
        }
    }

    void MobManager::spawnSampler(const World& world, const Vector3 centre, const float facingYaw) {
        // Line them up abreast ahead of the viewer, all turned to face back, so
        // the whole bestiary fits in one frame.
        const Vector3 fwd{std::cos(facingYaw), 0.0f, std::sin(facingYaw)};
        const Vector3 right{-std::sin(facingYaw), 0.0f, std::cos(facingYaw)};
        for (int i = 0; i < kMobKindCount; ++i) {
            const auto kind = static_cast<MobKind>(i);
            const float lateral = (static_cast<float>(i) - (kMobKindCount - 1) * 0.5f) * 2.4f;
            const int x = static_cast<int>(std::floor(centre.x + fwd.x * 9.0f + right.x * lateral));
            const int z = static_cast<int>(std::floor(centre.z + fwd.z * 9.0f + right.z * lateral));
            if (world.chunkAt(x, z) == nullptr)
                continue;
            Mob mob;
            mob.kind = kind;
            mob.pos = {static_cast<float>(x) + 0.5f,
                       static_cast<float>(world.surfaceHeight(x, z) + 1),
                       static_cast<float>(z) + 0.5f};
            mob.health = static_cast<float>(mobStats(kind).maxHealth);
            mob.yaw = facingYaw + PI; // look back at the camera
            mob.targetYaw = mob.yaw;
            mob.wanderTimer = 12.0f;  // hold still long enough to be inspected
            mobs_.push_back(mob);
        }
    }

    void MobManager::update(World& world, const Vector3 playerPos, const float dayFraction,
                            const float dt) {
        deaths.clear();

        // dayFraction is the raw 0..1 clock; convert to a sun factor.
        const float sunEl = std::sin(dayFraction * 2.0f * PI);
        const float dayFactor = std::clamp(0.5f + sunEl * 1.4f, 0.0f, 1.0f);

        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.0f) {
            spawnTimer_ = 0.8f;
            trySpawn_(world, playerPos, dayFactor);
        }

        for (Mob& mob : mobs_) {
            simulate_(world, mob, playerPos, dayFactor, dt);
        }

        // Retire corpses once they have finished toppling, and cull anything
        // that has drifted far outside the streamed world.
        std::erase_if(mobs_, [playerPos](const Mob& m) {
            if (m.dead && m.deathTimer > 0.9f)
                return true;
            const float dx = m.pos.x - playerPos.x;
            const float dz = m.pos.z - playerPos.z;
            return dx * dx + dz * dz > 110.0f * 110.0f;
        });
    }

    // ---------------------------------------------------------------- combat

    bool MobManager::attack(const Vector3 eye, const Vector3 dir, const float reach,
                            const int damage) {
        Mob* best = nullptr;
        float bestT = reach;
        for (Mob& mob : mobs_) {
            if (mob.dead)
                continue;
            // Ray vs. the mob's AABB, slightly inflated so swings feel generous.
            const BoundingBox box = mob.bounds();
            const Ray ray{eye, dir};
            const RayCollision hit = GetRayCollisionBox(ray, box);
            if (hit.hit && hit.distance < bestT) {
                bestT = hit.distance;
                best = &mob;
            }
        }
        if (best == nullptr)
            return false;

        best->health -= static_cast<float>(damage);
        best->hurtFlash = 1.0f;
        best->knockback = 1.0f;
        // Shove it away and make passive animals bolt.
        const Vector3 away = Vector3Normalize({dir.x, 0.0f, dir.z});
        best->vel.x += away.x * 6.0f;
        best->vel.z += away.z * 6.0f;
        best->vel.y = std::max(best->vel.y, 3.4f);
        if (!mobStats(best->kind).hostile) {
            best->targetYaw = std::atan2(away.z, away.x);
            best->moving = true;
            best->wanderTimer = 3.0f;
        }
        if (best->health <= 0.0f) {
            best->dead = true;
            deaths.push_back({best->centre(), best->kind});
        }
        return true;
    }

    // ----------------------------------------------------------------- draw

    void MobManager::draw(const Camera3D& cam, const float dayFactor) const {
        if (atlas_.id == 0 || mobs_.empty())
            return;

        rlSetTexture(atlas_.id);
        rlBegin(RL_QUADS);

        for (const Mob& mob : mobs_) {
            const MobStats& st = mobStats(mob.kind);
            const Model model = modelFor(mob.kind);

            // Cheap distance cull; mobs are small and dense detail far away is
            // wasted on the software rasteriser.
            const float dx = mob.pos.x - cam.position.x;
            const float dz = mob.pos.z - cam.position.z;
            if (dx * dx + dz * dz > 72.0f * 72.0f)
                continue;

            // Daylight plus a floor so mobs never turn into silhouettes.
            const float lit = std::clamp(0.28f + dayFactor * 0.72f, 0.0f, 1.0f);
            Color tint = {static_cast<unsigned char>(255 * lit),
                          static_cast<unsigned char>(255 * lit),
                          static_cast<unsigned char>(255 * lit), 255};
            if (mob.burning > 0.01f) {
                // Ember glow: bright and flickering, clearly different from the
                // red flash of taking a hit.
                const float flicker = 0.72f + 0.28f * std::sin(mob.limbPhase * 22.0f +
                                                               mob.pos.x * 4.0f);
                tint.r = 255;
                tint.g = static_cast<unsigned char>(std::clamp(150.0f * flicker, 0.0f, 255.0f));
                tint.b = static_cast<unsigned char>(std::clamp(40.0f * flicker, 0.0f, 255.0f));
            } else if (mob.hurtFlash > 0.01f) {
                const float f = mob.hurtFlash;
                tint.g = static_cast<unsigned char>(tint.g * (1.0f - f * 0.7f));
                tint.b = static_cast<unsigned char>(tint.b * (1.0f - f * 0.7f));
            }

            const float swing = std::sin(mob.limbPhase) * mob.limbSwing * 42.0f;
            const float headYaw = std::sin(mob.limbPhase * 0.3f) * 6.0f;
            const float deathRoll = mob.dead ? std::min(mob.deathTimer / 0.6f, 1.0f) * 90.0f : 0.0f;

            rlPushMatrix();
            rlTranslatef(mob.pos.x, mob.pos.y, mob.pos.z);
            rlRotatef(-mob.yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
            if (deathRoll > 0.0f) {
                // Topple sideways about the feet, like a felled mob.
                rlRotatef(deathRoll, 1.0f, 0.0f, 0.0f);
            }

            for (int i = 0; i < model.count; ++i) {
                const Part& part = model.parts[i];
                float angle = 0.0f;
                switch (part.swing) {
                case Swing::LegA: angle = swing; break;
                case Swing::LegB: angle = -swing; break;
                case Swing::ArmA: angle = -swing * 0.6f; break;
                case Swing::ArmB: angle = swing * 0.6f; break;
                case Swing::Head: angle = 0.0f; break;
                case Swing::None: break;
                }

                rlPushMatrix();
                if (part.swing == Swing::Head) {
                    // Head sway reads as the creature looking around.
                    const float pivotY = part.off.y;
                    rlTranslatef(0.0f, pivotY, 0.0f);
                    rlRotatef(headYaw, 0.0f, 1.0f, 0.0f);
                    rlTranslatef(0.0f, -pivotY, 0.0f);
                } else if (angle != 0.0f) {
                    // Hinge at the top of the limb, not its centre.
                    const float pivotY = part.off.y + part.size.y * 0.5f;
                    rlTranslatef(0.0f, pivotY, 0.0f);
                    rlRotatef(angle, 0.0f, 0.0f, 1.0f);
                    rlTranslatef(0.0f, -pivotY, 0.0f);
                }

                boxFaces(part.off, part.size, tileRect(part.tile),
                         part.faceTile >= 0 ? tileRect(part.faceTile) : Rectangle{0, 0, 0, 0}, tint);
                rlPopMatrix();
            }
            rlPopMatrix();
            (void)st;
        }

        rlEnd();
        rlSetTexture(0);
    }

} // namespace vox
