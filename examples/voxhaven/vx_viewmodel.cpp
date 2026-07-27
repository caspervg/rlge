#include "vx_viewmodel.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "rlgl.h"

#include "vx_items.hpp"

namespace vox {

    namespace {
        // Face shades matching the terrain bake, so the arm and the held block
        // sit in the same lighting language as everything around them.
        constexpr float kFaceShade[6] = {1.0f, 0.55f, 0.74f, 0.74f, 0.88f, 0.88f};

        Color shadeOf(const Color c, const float mul) {
            const auto ch = [](const float v) {
                return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
            };
            return {ch(c.r * mul), ch(c.g * mul), ch(c.b * mul), c.a};
        }

        // Emits a textured box in the current rlgl matrix. `faces` gives a tile
        // per face in the order +Y, -Y, +X, -X, +Z, -Z.
        void box(const Vector3 c, const Vector3 s, const int faces[6], const Color tint) {
            const float x0 = c.x - s.x * 0.5f, x1 = c.x + s.x * 0.5f;
            const float y0 = c.y - s.y * 0.5f, y1 = c.y + s.y * 0.5f;
            const float z0 = c.z - s.z * 0.5f, z1 = c.z + s.z * 0.5f;

            const auto quad = [&](const int f, const Vector3 a, const Vector3 b, const Vector3 d,
                                  const Vector3 e) {
                const Rectangle uv = tileUV(faces[f]);
                const Color col = shadeOf(tint, kFaceShade[f]);
                rlColor4ub(col.r, col.g, col.b, col.a);
                const float ix = uv.width * 0.02f, iy = uv.height * 0.02f;
                rlTexCoord2f(uv.x + ix, uv.y + uv.height - iy);            rlVertex3f(a.x, a.y, a.z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + uv.height - iy); rlVertex3f(b.x, b.y, b.z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + iy);             rlVertex3f(d.x, d.y, d.z);
                rlTexCoord2f(uv.x + ix, uv.y + iy);                        rlVertex3f(e.x, e.y, e.z);
            };

            quad(0, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0});
            quad(1, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1});
            quad(2, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1});
            quad(3, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0});
            quad(4, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1});
            quad(5, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0});
        }

        void uniformBox(const Vector3 c, const Vector3 s, const int tile, const Color tint) {
            const int faces[6] = {tile, tile, tile, tile, tile, tile};
            box(c, s, faces, tint);
        }

        // An item icon given a little depth: front and back faces only, offset
        // by `thick`. The four rim faces are deliberately skipped -- an icon
        // tile is mostly transparent, so texturing a sliver of it across the
        // rim just draws a dotted outline of the quad in mid-air.
        void itemSlab(const Vector3 c, const float size, const float thick, const int tile,
                      const Color tint) {
            const Rectangle uv = tileUV(tile);
            const float h = size * 0.5f, t = thick * 0.5f;
            const float ix = uv.width * 0.01f, iy = uv.height * 0.01f;

            const auto face = [&](const float z, const bool flip, const float shade) {
                const Color col = shadeOf(tint, shade);
                rlColor4ub(col.r, col.g, col.b, col.a);
                const float x0 = flip ? c.x + h : c.x - h;
                const float x1 = flip ? c.x - h : c.x + h;
                rlTexCoord2f(uv.x + ix, uv.y + uv.height - iy);
                rlVertex3f(x0, c.y - h, c.z + z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + uv.height - iy);
                rlVertex3f(x1, c.y - h, c.z + z);
                rlTexCoord2f(uv.x + uv.width - ix, uv.y + iy);
                rlVertex3f(x1, c.y + h, c.z + z);
                rlTexCoord2f(uv.x + ix, uv.y + iy);
                rlVertex3f(x0, c.y + h, c.z + z);
            };
            face(t, false, 1.0f);
            face(-t, true, 0.78f);
        }
    } // namespace

    void drawViewModel(const Camera3D& cam, const Texture2D atlas, const ViewModelState& vm) {
        // Camera basis. The view model lives in this space: +x right, +y up,
        // +z straight ahead, all relative to wherever the player is looking.
        const Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.up));
        const Vector3 up = Vector3CrossProduct(right, fwd);

        // Columns are the basis vectors, translation is the eye.
        Matrix basis{};
        basis.m0 = right.x; basis.m1 = right.y; basis.m2 = right.z; basis.m3 = 0.0f;
        basis.m4 = up.x;    basis.m5 = up.y;    basis.m6 = up.z;    basis.m7 = 0.0f;
        basis.m8 = fwd.x;   basis.m9 = fwd.y;   basis.m10 = fwd.z;  basis.m11 = 0.0f;
        basis.m12 = cam.position.x; basis.m13 = cam.position.y; basis.m14 = cam.position.z;
        basis.m15 = 1.0f;

        // Depth-only clear: the arm is not part of the world, so it must never
        // be clipped by a wall the player is standing against.
        rlDrawRenderBatchActive();
        rlColorMask(false, false, false, false);
        rlClearScreenBuffers();
        rlColorMask(true, true, true, true);

        const float lit = std::clamp(0.35f + vm.light * 0.65f, 0.0f, 1.0f);
        const Color tint{static_cast<unsigned char>(255 * lit),
                         static_cast<unsigned char>(255 * lit),
                         static_cast<unsigned char>(255 * lit), 255};

        // Swing follows a there-and-back arc; the bob is a lazy figure-of-eight.
        const float swingArc = std::sin(vm.swing * PI);
        const float bobX = std::cos(vm.bobPhase) * 0.016f * vm.bobAmount;
        const float bobY = std::sin(vm.bobPhase * 2.0f) * 0.012f * vm.bobAmount;
        const float drop = vm.equipDrop * 0.30f;

        rlSetTexture(atlas.id);
        rlBegin(RL_QUADS);
        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(basis));

        // Everything hangs off one hand anchor, so the arm and whatever it is
        // gripping swing as a single unit. kHandZ is far enough out that a
        // 70-degree FOV does not blow the arm up to fill a quarter of the
        // screen -- at this distance the frustum is ~1.6 units wide, and the
        // arm is a tenth of that.
        constexpr float kHandZ = 0.74f;
        rlPushMatrix();
        rlTranslatef(0.36f + bobX, -0.17f + bobY - drop, kHandZ);
        rlRotatef(-swingArc * 44.0f, 1.0f, 0.0f, 0.0f);           // punch upward
        rlTranslatef(0.0f, swingArc * 0.05f, swingArc * 0.09f);   // and forward

        // --- arm ---------------------------------------------------------
        constexpr float kArmTilt = 26.0f; // degrees the limb falls toward the viewer
        rlPushMatrix();
        rlRotatef(kArmTilt, 1.0f, 0.0f, 0.0f);  // falls away toward the viewer
        rlRotatef(-10.0f, 0.0f, 0.0f, 1.0f);    // splayed slightly outward
        {
            // Bare hand at the top, sleeve below it running off the bottom of
            // the frame the way a real first-person arm does. Keep the skin
            // segment short: past the wrist it is all sleeve.
            uniformBox({0.0f, -0.075f, 0.0f}, {0.100f, 0.15f, 0.100f}, kTileArmSkin, tint);
            const int sleeve[6] = {kTileSleeve, kTileSleeve, kTileSleeve,
                                   kTileSleeve, kTileSleeve, kTileSleeve};
            box({0.0f, -0.38f, 0.0f}, {0.104f, 0.46f, 0.104f}, sleeve, tint);
        }
        rlPopMatrix();

        // --- held item ---------------------------------------------------
        if (vm.held != Block::Air) {
            const BlockInfo& info = blockInfo(vm.held);
            rlPushMatrix();
            // Ride part of the way down to the fist. Following it all the way
            // reads as "held" in world space but buries the item behind the
            // hand on screen, since the hand is nearer the eye.
            constexpr float kFistY = 0.035f;
            rlTranslatef(0.0f, -kFistY * std::cos(kArmTilt * DEG2RAD),
                         -kFistY * std::sin(kArmTilt * DEG2RAD));

            if (isItem(vm.held)) {
                // Every icon is drawn with its grip in the lower-left quarter
                // of the tile, so offsetting the slab by a quarter of its size
                // puts that grip on the origin -- which is the fist. The item
                // then pivots around the hand instead of orbiting it.
                constexpr float kSize = 0.19f;
                // Slightly nearer the eye than the fist, so the blade crosses
                // in front of the fingers instead of being swallowed by them.
                rlTranslatef(0.0f, 0.02f, -0.06f);
                rlRotatef(-18.0f, 0.0f, 0.0f, 1.0f);
                rlRotatef(30.0f, 0.0f, 1.0f, 0.0f);
                itemSlab({kSize * 0.25f, kSize * 0.25f, 0.0f}, kSize, 0.02f, info.tileSide, tint);
            } else {
                // A block is held as a block, showing top and side like the
                // hotbar icon does.
                rlTranslatef(-0.01f, 0.02f, 0.06f);
                rlRotatef(-26.0f, 0.0f, 1.0f, 0.0f);
                rlRotatef(10.0f, 1.0f, 0.0f, 0.0f);
                const int faces[6] = {info.tileTop, info.tileBottom, info.tileSide,
                                      info.tileSide, info.tileSide, info.tileSide};
                box({-0.02f, 0.03f, 0.0f}, {0.165f, 0.165f, 0.165f}, faces, tint);
            }
            rlPopMatrix();
        }

        rlPopMatrix();

        rlPopMatrix();
        rlEnd();
        rlSetTexture(0);
        rlDrawRenderBatchActive();
    }

} // namespace vox
