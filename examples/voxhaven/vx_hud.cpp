#include "vx_hud.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include "vx_config.hpp"

namespace vox {

    namespace {

        // --------------------------------------------------------- Palette
        // vox::pal is the base; these extend it with the extra tones a layered
        // dark UI needs (panel depth, edges, semantic states). Kept local so
        // the shared config header stays owned by the scene.
        constexpr Color kPanelDeep{14, 16, 25, 242};
        constexpr Color kPanelMid{28, 32, 46, 244};
        constexpr Color kPanelWell{9, 10, 16, 210};   // recessed areas (slots, tracks)
        constexpr Color kEdge{88, 96, 126, 255};
        constexpr Color kEdgeSoft{58, 64, 88, 255};
        constexpr Color kAccent = pal::hudAccent;
        constexpr Color kAccentDeep{158, 112, 30, 255};
        constexpr Color kDanger{224, 96, 78, 255};
        constexpr Color kGood{126, 206, 138, 255};
        constexpr Color kWaterTint{34, 96, 190, 255};

        // ----------------------------------------------------- Tiny helpers

        Color mix(const Color a, const Color b, const float t) {
            const auto ch = [t](const unsigned char x, const unsigned char y) {
                return static_cast<unsigned char>(
                    std::clamp(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t,
                               0.0f, 255.0f));
            };
            return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
        }

        // Corner radius in pixels -> raylib's relative roundness parameter.
        float roundnessFor(const Rectangle r, const float radius) {
            const float half = std::min(r.width, r.height) * 0.5f;
            if (half <= 0.001f)
                return 0.0f;
            return std::clamp(radius / half, 0.0f, 1.0f);
        }

        void roundRect(const Rectangle r, const float radius, const Color c) {
            if (r.width <= 0.0f || r.height <= 0.0f)
                return;
            DrawRectangleRounded(r, roundnessFor(r, radius), 8, c);
        }

        void roundRectLines(const Rectangle r, const float radius, const float thick, const Color c) {
            if (r.width <= 0.0f || r.height <= 0.0f)
                return;
            DrawRectangleRoundedLinesEx(r, roundnessFor(r, radius), 8, thick, c);
        }

        Rectangle inset(const Rectangle r, const float d) {
            return {r.x + d, r.y + d, r.width - d * 2.0f, r.height - d * 2.0f};
        }

        // A solid panel: drop shadow, fill, faint top sheen, crisp border. The
        // sheen is what stops large dark rectangles reading as flat holes.
        void panel(const Rectangle r, const float s, const Color fill = kPanelDeep,
                   const Color edge = kEdge, const float a = 1.0f) {
            const float rad = 10.0f * s;
            roundRect({r.x + 3.0f * s, r.y + 5.0f * s, r.width, r.height}, rad, Fade(BLACK, 0.40f * a));
            roundRect(r, rad, Fade(fill, (fill.a / 255.0f) * a));
            const Rectangle sheen{r.x + 2.0f * s, r.y + 2.0f * s, r.width - 4.0f * s, r.height * 0.34f};
            roundRect(sheen, rad, Fade(WHITE, 0.030f * a));
            roundRectLines(r, rad, std::max(1.0f, 2.0f * s), Fade(edge, a));
        }

        // Recessed well used behind slots, sliders and the sparkline.
        void well(const Rectangle r, const float s, const float a = 1.0f) {
            roundRect(r, 6.0f * s, Fade(kPanelWell, (kPanelWell.a / 255.0f) * a));
            roundRectLines(r, 6.0f * s, std::max(1.0f, 1.0f * s), Fade(BLACK, 0.45f * a));
        }

        // Installed typefaces. Both fall back to raylib's built-in font, whose
        // glyphs are far tighter, so the tracking is chosen per font: a real
        // TTF already carries its own advance widths and only wants a hair of
        // extra letter-spacing.
        Font g_uiFont{};
        Font g_displayFont{};

        [[nodiscard]] Font uiFont() {
            return g_uiFont.texture.id != 0 ? g_uiFont : GetFontDefault();
        }

        [[nodiscard]] Font displayFont() {
            if (g_displayFont.texture.id != 0)
                return g_displayFont;
            return uiFont();
        }

        [[nodiscard]] float tracking(const Font& f, const float size) {
            return f.texture.id != 0 ? size * 0.035f : size / 10.0f;
        }

        void textAt(const Vector2 at, const char* t, const float size, const Color c) {
            const Font f = uiFont();
            DrawTextEx(f, t, at, size, tracking(f, size), c);
        }

        Vector2 measure(const char* t, const float size) {
            const Font f = uiFont();
            return MeasureTextEx(f, t, size, tracking(f, size));
        }

        void displayAt(const Vector2 at, const char* t, const float size, const Color c) {
            const Font f = displayFont();
            DrawTextEx(f, t, at, size, tracking(f, size), c);
        }

        Vector2 displayMeasure(const char* t, const float size) {
            const Font f = displayFont();
            return MeasureTextEx(f, t, size, tracking(f, size));
        }

        // Terrain is busy and bright; every HUD string gets a shadow so it stays
        // readable over snow, sand and sky alike.
        void textShadow(const Vector2 at, const char* t, const float size, const Color c,
                        const float shadow = 0.70f) {
            const float o = std::max(1.0f, size * 0.09f);
            textAt({at.x + o, at.y + o}, t, size, Fade(BLACK, shadow * (c.a / 255.0f)));
            textAt(at, t, size, c);
        }

        void textCentered(const float cx, const float y, const char* t, const float size, const Color c,
                          const bool shadow = true) {
            const Vector2 e = measure(t, size);
            const Vector2 at{cx - e.x * 0.5f, y};
            if (shadow)
                textShadow(at, t, size, c);
            else
                textAt(at, t, size, c);
        }

        void textRight(const float rx, const float y, const char* t, const float size, const Color c) {
            const Vector2 e = measure(t, size);
            textShadow({rx - e.x, y}, t, size, c);
        }

        // Four edge gradients. Cheaper and softer-looking than a radial texture,
        // and the corners darken twice which is exactly the vignette falloff.
        void vignette(const Rectangle vp, const Color c, const float strength) {
            const float w = vp.width * 0.26f;
            const float h = vp.height * 0.30f;
            const Color e = Fade(c, strength);
            DrawRectangleGradientEx({vp.x, vp.y, vp.width, h}, e, BLANK, e, BLANK);
            DrawRectangleGradientEx({vp.x, vp.y + vp.height - h, vp.width, h}, BLANK, e, BLANK, e);
            DrawRectangleGradientEx({vp.x, vp.y, w, vp.height}, e, e, BLANK, BLANK);
            DrawRectangleGradientEx({vp.x + vp.width - w, vp.y, w, vp.height}, BLANK, BLANK, e, e);
        }

        // Small labelled pill (status flags, creative badge, tooltips headers).
        float chipWidth(const char* label, const float size) {
            return measure(label, size).x + size * 1.1f;
        }

        void drawChip(const Rectangle r, const char* label, const float size, const Color accent,
                      const float a = 1.0f) {
            roundRect(r, r.height * 0.5f, Fade(mix(kPanelMid, accent, 0.18f), 0.90f * a));
            roundRectLines(r, r.height * 0.5f, std::max(1.0f, size * 0.09f), Fade(accent, 0.85f * a));
            const Vector2 e = measure(label, size);
            textAt({r.x + (r.width - e.x) * 0.5f, r.y + (r.height - e.y) * 0.5f}, label, size,
                   Fade(accent, a));
        }

        // Keyboard-key looking chip for the controls legend.
        void drawKeyCap(const Rectangle r, const char* key, const float size) {
            roundRect({r.x, r.y + 2.0f, r.width, r.height}, r.height * 0.28f, Fade(BLACK, 0.55f));
            roundRect(r, r.height * 0.28f, Fade(kPanelMid, 0.98f));
            roundRectLines(r, r.height * 0.28f, std::max(1.0f, size * 0.10f), Fade(kEdge, 0.95f));
            const Vector2 e = measure(key, size);
            textAt({r.x + (r.width - e.x) * 0.5f, r.y + (r.height - e.y) * 0.5f}, key, size, pal::hudText);
        }

        // ------------------------------------------------------ Clock dial

        // Scene time: dayFraction 0 == sunrise, so wall-clock hour = 6 + 24f.
        float clockHours(const float dayFraction) {
            return std::fmod(6.0f + dayFraction * 24.0f, 24.0f);
        }

        void drawClockDial(const Vector2 center, const float radius, const float dayFraction,
                           const float s) {
            const float ang = dayFraction * 2.0f * PI;
            const float elev = std::sin(ang);

            // Dial face: sky above the horizon line, night below it.
            DrawCircleV({center.x + 2.0f * s, center.y + 3.0f * s}, radius + 1.0f, Fade(BLACK, 0.40f));
            const float k = std::clamp((elev + 0.25f) / 0.6f, 0.0f, 1.0f);
            const Color upper = mix(pal::skyNight, pal::skyDay, k);
            DrawCircleV(center, radius, Fade(mix(kPanelDeep, upper, 0.55f), 0.96f));
            DrawCircleSector(center, radius, 0.0f, 180.0f, 24, Fade(pal::skyNight, 0.55f));
            DrawLineEx({center.x - radius, center.y}, {center.x + radius, center.y},
                       std::max(1.0f, 1.5f * s), Fade(kEdgeSoft, 0.9f));

            // Sunrise on the left, noon at the top, sunset on the right.
            const Vector2 body{center.x - std::cos(ang) * radius * 0.68f,
                               center.y - std::sin(ang) * radius * 0.68f};
            if (elev >= 0.0f) {
                DrawCircleV(body, radius * 0.30f, Fade(kAccent, 0.35f));
                DrawCircleV(body, radius * 0.19f, kAccent);
            } else {
                DrawCircleV(body, radius * 0.17f, Color{226, 232, 246, 255});
            }
            DrawCircleLinesV(center, radius, Fade(kEdge, 0.95f));
            DrawCircleLinesV(center, radius - 1.0f, Fade(kEdge, 0.45f));
        }

        // -------------------------------------------------------- Sparkline

        void drawSparkline(const Rectangle r, const float* samples, const int head, const float s) {
            constexpr int n = 64;
            float maxV = 60.0f;
            for (int i = 0; i < n; ++i)
                maxV = std::max(maxV, samples[i]);
            if (maxV <= 0.0f)
                return;

            well(r, s);
            // 60 FPS reference so the bars mean something at a glance.
            const float ref = r.y + r.height * (1.0f - 60.0f / maxV);
            DrawLineEx({r.x, ref}, {r.x + r.width, ref}, 1.0f, Fade(kEdgeSoft, 0.8f));

            const float bw = r.width / static_cast<float>(n);
            for (int i = 0; i < n; ++i) {
                const float v = samples[(head + i) % n];
                if (v <= 0.0f)
                    continue;
                const float h = std::clamp(v / maxV, 0.0f, 1.0f) * (r.height - 2.0f);
                const Color c = v >= 50.0f ? kGood : (v >= 28.0f ? kAccent : kDanger);
                DrawRectangleRec({r.x + static_cast<float>(i) * bw, r.y + r.height - 1.0f - h,
                                  std::max(1.0f, bw - 1.0f), h},
                                 Fade(c, 0.85f));
            }
        }

        // -------------------------------------------------------- Menu list

        struct MenuEntry {
            const char* label;
            MenuAction action;
        };

        // Shared keyboard+mouse entry list. `area` gives the centre-x and the
        // top of the first row; the list sizes itself from `fontSize`.
        MenuAction menuList(const HudContext& ctx, MenuState& st, const Rectangle area,
                            const std::span<const MenuEntry> entries, const float fontSize,
                            const float s) {
            const int count = static_cast<int>(entries.size());
            if (count == 0)
                return MenuAction::None;

            const float rowH = fontSize * 2.05f;
            const float gap = fontSize * 0.42f;
            const float cx = area.x + area.width * 0.5f;
            const float w = area.width;

            // Any real cursor movement hands the highlight to the mouse; the
            // arrow keys take it straight back on the next press.
            const float moved = std::fabs(ctx.mouse.x - st.lastMouse.x) +
                                std::fabs(ctx.mouse.y - st.lastMouse.y);
            if (moved > 1.5f)
                st.mouseOwnsHighlight = true;
            st.lastMouse = ctx.mouse;

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                st.index = (st.index - 1 + count) % count;
                st.mouseOwnsHighlight = false;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                st.index = (st.index + 1) % count;
                st.mouseOwnsHighlight = false;
            }
            st.index = std::clamp(st.index, 0, count - 1);

            int hovered = -1;
            for (int i = 0; i < count; ++i) {
                const Rectangle r{cx - w * 0.5f, area.y + static_cast<float>(i) * (rowH + gap), w, rowH};
                if (CheckCollisionPointRec(ctx.mouse, r))
                    hovered = i;
            }
            if (hovered >= 0 && st.mouseOwnsHighlight)
                st.index = hovered;

            MenuAction fired = MenuAction::None;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE))
                fired = entries[static_cast<std::size_t>(st.index)].action;
            if (ctx.mousePressed && hovered >= 0)
                fired = entries[static_cast<std::size_t>(hovered)].action;

            // Pop-in: rows slide up and fade, staggered by index.
            const float age = ctx.time - st.openTime;
            for (int i = 0; i < count; ++i) {
                const float t = std::clamp((age - static_cast<float>(i) * 0.045f) / 0.22f, 0.0f, 1.0f);
                const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
                const bool sel = i == st.index;
                const float slide = (1.0f - e) * 18.0f * s + (sel ? 6.0f * s : 0.0f);
                Rectangle r{cx - w * 0.5f + slide, area.y + static_cast<float>(i) * (rowH + gap), w, rowH};

                const Color fill = sel ? mix(kPanelMid, kAccent, 0.14f) : kPanelDeep;
                roundRect({r.x + 2.0f * s, r.y + 3.0f * s, r.width, r.height}, 8.0f * s,
                          Fade(BLACK, 0.35f * e));
                roundRect(r, 8.0f * s, Fade(fill, 0.94f * e));
                roundRectLines(r, 8.0f * s, std::max(1.0f, (sel ? 2.4f : 1.4f) * s),
                               Fade(sel ? kAccent : kEdgeSoft, (sel ? 0.95f : 0.7f) * e));
                if (sel) {
                    // Chunky accent tab on the left edge - the strongest cue.
                    roundRect({r.x + 5.0f * s, r.y + rowH * 0.22f, 5.0f * s, rowH * 0.56f}, 3.0f * s,
                              Fade(kAccent, e));
                }

                const char* label = entries[static_cast<std::size_t>(i)].label;
                const Vector2 le = measure(label, fontSize);
                const Vector2 at{r.x + (r.width - le.x) * 0.5f, r.y + (r.height - le.y) * 0.5f};
                textShadow(at, label, fontSize, Fade(sel ? pal::hudText : pal::hudDim, e));
            }

            if (fired != MenuAction::None)
                st.pending = fired;
            return fired;
        }

        // ------------------------------------------------- Inventory layout

        struct InvLayout {
            Rectangle panel{};
            Rectangle grid{};    // storage area
            Rectangle hotbar{};  // hotbar row
            float slot = 0.0f;
            float gap = 0.0f;
            float pad = 0.0f;
            float titleH = 0.0f;
        };

        InvLayout inventoryLayout(const Rectangle vp, const float s) {
            InvLayout L;
            L.slot = 46.0f * s;
            L.gap = 5.0f * s;
            L.pad = 18.0f * s;
            L.titleH = 34.0f * s;
            const float gridW = L.slot * 9.0f + L.gap * 8.0f;
            const float rowsH = L.slot * 3.0f + L.gap * 2.0f;
            const float sepH = 20.0f * s;
            const float w = gridW + L.pad * 2.0f;
            const float h = L.titleH + rowsH + sepH + L.slot + L.pad * 2.0f;
            L.panel = {vp.x + (vp.width - w) * 0.5f, vp.y + (vp.height - h) * 0.5f - 12.0f * s, w, h};
            L.grid = {L.panel.x + L.pad, L.panel.y + L.pad + L.titleH, gridW, rowsH};
            L.hotbar = {L.grid.x, L.grid.y + rowsH + sepH, gridW, L.slot};
            return L;
        }

        // Flat index (0..8 hotbar, 9..35 storage) -> screen rect.
        Rectangle invSlotRect(const InvLayout& L, const int index) {
            if (index < kHotbarSlots) {
                return {L.hotbar.x + static_cast<float>(index) * (L.slot + L.gap), L.hotbar.y, L.slot,
                        L.slot};
            }
            const int i = index - kHotbarSlots;
            const int col = i % 9;
            const int row = i / 9;
            return {L.grid.x + static_cast<float>(col) * (L.slot + L.gap),
                    L.grid.y + static_cast<float>(row) * (L.slot + L.gap), L.slot, L.slot};
        }

        // ------------------------------------------------------ Slot drawing

        void drawSlotBase(const Rectangle r, const float s, const bool highlight, const float a) {
            roundRect(r, 6.0f * s, Fade(pal::hudSlot, (pal::hudSlot.a / 255.0f) * a));
            // Inner bevel: light top, dark bottom - reads as a physical socket.
            DrawRectangleGradientEx(inset(r, 2.0f * s), Fade(WHITE, 0.075f * a), BLANK,
                                    Fade(WHITE, 0.075f * a), BLANK);
            DrawRectangleGradientEx({r.x, r.y + r.height * 0.55f, r.width, r.height * 0.45f}, BLANK,
                                    Fade(BLACK, 0.22f * a), BLANK, Fade(BLACK, 0.22f * a));
            roundRectLines(r, 6.0f * s, std::max(1.0f, (highlight ? 2.4f : 1.2f) * s),
                           Fade(highlight ? kAccent : kEdgeSoft, (highlight ? 0.95f : 0.75f) * a));
        }

        void drawStackCount(const Rectangle r, const int count, const float s, const float a) {
            if (count <= 1)
                return;
            const float size = std::max(11.0f, 15.0f * s);
            const char* t = TextFormat("%d", count);
            const Vector2 e = measure(t, size);
            const Vector2 at{r.x + r.width - e.x - 3.0f * s, r.y + r.height - e.y - 2.0f * s};
            const float o = std::max(1.0f, size * 0.12f);
            textAt({at.x + o, at.y + o}, t, size, Fade(BLACK, 0.85f * a));
            textAt(at, t, size, Fade(count >= kMaxStack ? kAccent : pal::hudText, a));
        }

    } // namespace

    // ------------------------------------------------------- Public helpers

    void fillMouseFromRaylib(HudContext& ctx) {
        ctx.mouse = GetMousePosition();
        ctx.mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        ctx.mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        ctx.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        ctx.mouseRightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        ctx.mouseWheel = GetMouseWheelMove();
    }

    void setHudFonts(const Font ui, const Font display) {
        g_uiFont = ui;
        g_displayFont = display;
    }

    void hudText(const Vector2 at, const char* text, const float size, const Color color) {
        textAt(at, text, size, color);
    }

    void hudTextShadow(const Vector2 at, const char* text, const float size, const Color color) {
        textShadow(at, text, size, color);
    }

    Vector2 hudMeasure(const char* text, const float size) {
        return measure(text, size);
    }

    float hudScale(const Rectangle viewport) {
        // Reference layout is 1280x720; clamp so tiny and huge windows stay usable.
        const float k = std::min(viewport.width / 1280.0f, viewport.height / 720.0f);
        return std::clamp(k, 0.72f, 2.25f);
    }

    void drawBlockIcon(const Texture2D atlas, const Block b, const Rectangle dst, const float alpha) {
        if (b == Block::Air || atlas.id == 0 || dst.width <= 0.0f || dst.height <= 0.0f)
            return;
        const BlockInfo& info = blockInfo(b);
        const Color tint = Fade(WHITE, alpha);
        const auto srcOf = [&atlas](const int tile) {
            const Rectangle uv = tileUV(tile);
            return Rectangle{uv.x * static_cast<float>(atlas.width),
                             uv.y * static_cast<float>(atlas.height),
                             uv.width * static_cast<float>(atlas.width),
                             uv.height * static_cast<float>(atlas.height)};
        };

        DrawTexturePro(atlas, srcOf(info.tileSide), dst, {0, 0}, 0.0f, tint);
        if (info.tileTop != info.tileSide) {
            // Squash the top face into a band along the upper edge so blocks
            // with a distinct cap (grass, snow, wood) are identifiable at 30px.
            const float capH = dst.height * 0.36f;
            DrawTexturePro(atlas, srcOf(info.tileTop), {dst.x, dst.y, dst.width, capH}, {0, 0}, 0.0f,
                           tint);
            DrawLineEx({dst.x, dst.y + capH}, {dst.x + dst.width, dst.y + capH}, 1.0f,
                       Fade(BLACK, 0.22f * alpha));
        }
        // Fake cube lighting so flat tiles do not look like stickers.
        DrawRectangleGradientEx({dst.x, dst.y, dst.width, dst.height * 0.30f},
                                Fade(WHITE, 0.16f * alpha), BLANK, Fade(WHITE, 0.16f * alpha), BLANK);
        DrawRectangleGradientEx({dst.x, dst.y + dst.height * 0.60f, dst.width, dst.height * 0.40f},
                                BLANK, Fade(BLACK, 0.26f * alpha), BLANK, Fade(BLACK, 0.26f * alpha));
    }

    // -------------------------------------------------------------- In-game HUD

    void drawHud(const HudContext& ctx) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const float cx = vp.x + vp.width * 0.5f;
        const float cy = vp.y + vp.height * 0.5f;

        // --- Mood layers ---------------------------------------------------
        vignette(vp, BLACK, 0.22f);
        if (ctx.underwater) {
            DrawRectangleRec(vp, Fade(kWaterTint, 0.30f));
            vignette(vp, Color{8, 30, 76, 255}, 0.40f);
        }

        // --- Crosshair -----------------------------------------------------
        {
            const float pulse = ctx.hasTarget ? (0.5f + 0.5f * std::sin(ctx.time * 6.0f)) : 0.0f;
            const float gap = (5.0f + (ctx.hasTarget ? 2.5f : 0.0f) + pulse * 0.8f) * s;
            const float len = 7.0f * s;
            const float th = std::max(2.0f, 2.0f * s);
            const Color c = ctx.hasTarget ? mix(WHITE, kAccent, 0.65f) : WHITE;
            const Color shadow = Fade(BLACK, 0.45f);
            const auto arm = [&](const Vector2 a, const Vector2 b) {
                DrawLineEx({a.x + 1.0f, a.y + 1.0f}, {b.x + 1.0f, b.y + 1.0f}, th + 1.0f, shadow);
                DrawLineEx(a, b, th, Fade(c, 0.92f));
            };
            arm({cx - gap - len, cy}, {cx - gap, cy});
            arm({cx + gap, cy}, {cx + gap + len, cy});
            arm({cx, cy - gap - len}, {cx, cy - gap});
            arm({cx, cy + gap}, {cx, cy + gap + len});
            DrawCircleV({cx, cy}, std::max(1.0f, 1.3f * s), Fade(c, 0.85f));

            if (ctx.breakProgress > 0.0f) {
                const float r0 = gap + len + 4.0f * s;
                const float r1 = r0 + 4.0f * s;
                DrawRing({cx, cy}, r0, r1, 0.0f, 360.0f, 48, Fade(BLACK, 0.45f));
                DrawRing({cx, cy}, r0, r1, -90.0f, -90.0f + 360.0f * std::clamp(ctx.breakProgress, 0.0f, 1.0f),
                         48, Fade(kAccent, 0.95f));
            }
            if (ctx.lookingAtName != nullptr && ctx.lookingAtName[0] != '\0') {
                textCentered(cx, cy + 26.0f * s, ctx.lookingAtName, 14.0f * s, Fade(pal::hudDim, 0.85f));
            }
        }

        // --- Hotbar --------------------------------------------------------
        const float slot = 52.0f * s;
        const float gap = 4.0f * s;
        const float barW = slot * 9.0f + gap * 8.0f;
        const float barX = cx - barW * 0.5f;
        const float barY = vp.y + vp.height - slot - 24.0f * s;
        {
            const float pad = 7.0f * s;
            panel({barX - pad, barY - pad, barW + pad * 2.0f, slot + pad * 2.0f}, s, kPanelDeep, kEdge);

            const Inventory* inv = ctx.inventory;
            const int sel = inv ? inv->selected() : 0;
            for (int i = 0; i < kHotbarSlots; ++i) {
                Rectangle r{barX + static_cast<float>(i) * (slot + gap), barY, slot, slot};
                const bool isSel = i == sel;
                if (isSel) {
                    // Lift the selected slot out of the row instead of only
                    // recolouring it: motion reads faster than colour.
                    r = {r.x - 3.0f * s, r.y - 5.0f * s, r.width + 6.0f * s, r.height + 6.0f * s};
                    roundRect(inset(r, -4.0f * s), 10.0f * s, Fade(kAccent, 0.16f));
                }
                drawSlotBase(r, s, isSel, 1.0f);

                const Slot cell = inv ? inv->slot(i) : Slot{};
                if (!cell.empty()) {
                    drawBlockIcon(ctx.atlas, cell.block, inset(r, 8.0f * s), 1.0f);
                    drawStackCount(r, cell.count, s, 1.0f);
                }
                // Slot number hint, brighter on the active slot.
                textAt({r.x + 4.0f * s, r.y + 2.0f * s}, TextFormat("%d", i + 1), 11.0f * s,
                       Fade(isSel ? kAccent : WHITE, isSel ? 0.9f : 0.35f));
            }

            // Floating name of the selected block, fading out after a beat.
            if (inv != nullptr && !inv->slot(sel).empty()) {
                const float age = ctx.time - ctx.selectionChangedAt;
                const float a = std::clamp(1.0f - (age - 1.6f) / 0.7f, 0.0f, 1.0f) *
                                std::clamp(age / 0.10f, 0.0f, 1.0f);
                if (a > 0.01f) {
                    const char* name = blockInfo(inv->slot(sel).block).name;
                    const float fs = 18.0f * s;
                    const Vector2 e = measure(name, fs);
                    const Rectangle pill{cx - e.x * 0.5f - fs * 0.6f, barY - pad - fs * 1.9f,
                                         e.x + fs * 1.2f, fs * 1.5f};
                    roundRect(pill, pill.height * 0.5f, Fade(kPanelDeep, 0.86f * a));
                    roundRectLines(pill, pill.height * 0.5f, std::max(1.0f, 1.4f * s),
                                   Fade(kAccentDeep, 0.8f * a));
                    textAt({cx - e.x * 0.5f, pill.y + (pill.height - e.y) * 0.5f}, name, fs,
                           Fade(pal::hudText, a));
                }
            }

            // Optional hearts row, left-aligned above the bar.
            if (ctx.showHealth && ctx.maxHealth > 0 && !ctx.creative) {
                const float hs = 13.0f * s;
                const int hearts = std::max(1, ctx.maxHealth / 2);
                for (int i = 0; i < hearts; ++i) {
                    const float x = barX + static_cast<float>(i) * (hs + 3.0f * s);
                    const float y = barY - pad - hs - 6.0f * s;
                    const int filled = std::clamp(ctx.health - i * 2, 0, 2);
                    DrawRectangleRec({x, y, hs, hs}, Fade(BLACK, 0.55f));
                    if (filled > 0) {
                        DrawRectangleRec({x + 1.0f, y + 1.0f, (hs - 2.0f) * (filled == 2 ? 1.0f : 0.5f),
                                          hs - 2.0f},
                                         kDanger);
                    }
                }
            }
        }

        // --- Clock dial + status chips (top right) --------------------------
        {
            const float rad = 26.0f * s;
            const Vector2 dial{vp.x + vp.width - 20.0f * s - rad, vp.y + 20.0f * s + rad};
            drawClockDial(dial, rad, ctx.dayFraction, s);

            const float hours = clockHours(ctx.dayFraction);
            const int hh = static_cast<int>(hours);
            const int mm = static_cast<int>((hours - static_cast<float>(hh)) * 60.0f);
            textCentered(dial.x, dial.y + rad + 5.0f * s, TextFormat("%02d:%02d", hh, mm), 15.0f * s,
                         pal::hudText);

            // Right-aligned stack of status pills under the dial.
            const float fs = 12.0f * s;
            float chipY = dial.y + rad + 26.0f * s;
            const float rightX = vp.x + vp.width - 20.0f * s;
            const auto pill = [&](const char* label, const Color col) {
                const float w = chipWidth(label, fs);
                drawChip({rightX - w, chipY, w, fs * 1.75f}, label, fs, col);
                chipY += fs * 2.1f;
            };
            if (ctx.creative)
                pill("CREATIVE", kAccent);
            if (ctx.flying)
                pill("FLYING", Color{140, 196, 255, 255});
            if (ctx.underwater)
                pill("SWIMMING", Color{120, 200, 232, 255});
            if (ctx.sprinting)
                pill("SPRINT", kGood);
        }

        // --- Info strip (top left), opt-in ----------------------------------
        if (!settings.showDebugLine)
            return;

        const float pw = 306.0f * s;
        const float ph = 88.0f * s;
        const Rectangle info{vp.x + 18.0f * s, vp.y + 18.0f * s, pw, ph};
        panel(info, s, kPanelDeep, kEdgeSoft);

        const float ix = info.x + 14.0f * s;
        const float fpsSize = 30.0f * s;
        const Color fpsCol = ctx.fps >= 50 ? kGood : (ctx.fps >= 28 ? kAccent : kDanger);
        textShadow({ix, info.y + 10.0f * s}, TextFormat("%d", ctx.fps), fpsSize, fpsCol);
        const float fpsW = measure(TextFormat("%d", ctx.fps), fpsSize).x;
        textAt({ix + fpsW + 5.0f * s, info.y + 10.0f * s + fpsSize * 0.52f}, "FPS", 12.0f * s,
               Fade(pal::hudDim, 0.9f));

        drawSparkline({info.x + info.width - 108.0f * s, info.y + 10.0f * s, 94.0f * s, 30.0f * s},
                      ctx.fpsGraphSamples, ctx.fpsGraphHead, s);

        textShadow({ix, info.y + 46.0f * s},
                   TextFormat("XYZ  %.1f  %.1f  %.1f", ctx.playerPos.x, ctx.playerPos.y, ctx.playerPos.z),
                   14.0f * s, pal::hudText);

        const char* biome = (ctx.biomeName != nullptr && ctx.biomeName[0] != '\0') ? ctx.biomeName : "-";
        textShadow({ix, info.y + 66.0f * s},
                   TextFormat("%s   chunks %d/%d   mesh %d", biome, ctx.chunksDrawn, ctx.chunksLoaded,
                              ctx.meshedThisFrame),
                   12.0f * s, Fade(pal::hudDim, 0.95f));
    }

    // ------------------------------------------------------- Inventory screen

    void returnHeldToInventory(Inventory& inv, InventoryUiState& ui) {
        if (ui.held.empty()) {
            ui.held.clear();
            return;
        }
        // Keep whatever did not fit so nothing silently disappears.
        ui.held.count = inv.addLeftover(ui.held.block, ui.held.count);
        if (ui.held.count <= 0)
            ui.held.clear();
    }

    void drawInventoryScreen(const HudContext& ctx, Inventory& inv, InventoryUiState& ui) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const InvLayout L = inventoryLayout(vp, s);

        // Pop-in: fade + a small upward slide. Hit tests use the same offset so
        // clicks always land on what the player sees.
        const float t = std::clamp((ctx.time - ui.openTime) / 0.18f, 0.0f, 1.0f);
        const float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        const float dy = (1.0f - e) * 16.0f * s;
        const auto shift = [dy](Rectangle r) {
            r.y += dy;
            return r;
        };

        DrawRectangleRec(vp, Fade(BLACK, 0.62f * e));
        vignette(vp, BLACK, 0.30f * e);

        // --- Hover + click handling ----------------------------------------
        ui.hovered = -1;
        for (int i = 0; i < kInventorySlots; ++i) {
            if (CheckCollisionPointRec(ctx.mouse, shift(invSlotRect(L, i)))) {
                ui.hovered = i;
                break;
            }
        }
        if (ctx.mousePressed) {
            if (ui.hovered >= 0) {
                inv.clickSlot(ui.hovered, ui.held, false);
            } else if (!CheckCollisionPointRec(ctx.mouse, shift(L.panel)) && !ui.held.empty()) {
                returnHeldToInventory(inv, ui); // clicking the void puts items back
            }
        } else if (ctx.mouseRightPressed && ui.hovered >= 0) {
            inv.clickSlot(ui.hovered, ui.held, true);
        }

        // --- Panel ----------------------------------------------------------
        const Rectangle pr = shift(L.panel);
        panel(pr, s, kPanelDeep, kEdge, e);

        const float titleSize = 20.0f * s;
        textShadow({pr.x + L.pad, pr.y + L.pad * 0.55f}, "INVENTORY", titleSize,
                   Fade(pal::hudText, e));
        if (ctx.creative) {
            const float fs = 12.0f * s;
            const float w = chipWidth("CREATIVE", fs);
            drawChip({pr.x + pr.width - L.pad - w, pr.y + L.pad * 0.55f, w, fs * 1.75f}, "CREATIVE", fs,
                     kAccent, e);
        }
        // Accent rule under the title ties the header to the palette.
        DrawRectangleRec({pr.x + L.pad, pr.y + L.pad * 0.55f + titleSize * 1.35f, pr.width - L.pad * 2.0f,
                          std::max(1.0f, 2.0f * s)},
                         Fade(kAccentDeep, 0.75f * e));

        // Wells behind each region so the grid reads as one object.
        well(inset(shift(L.grid), -6.0f * s), s, e);
        well(inset(shift(L.hotbar), -6.0f * s), s, e);
        textAt({shift(L.hotbar).x, shift(L.hotbar).y - 15.0f * s}, "HOTBAR", 11.0f * s,
               Fade(pal::hudDim, 0.8f * e));

        const int sel = inv.selected();
        for (int i = 0; i < kInventorySlots; ++i) {
            const Rectangle r = shift(invSlotRect(L, i));
            const bool hot = i == ui.hovered;
            drawSlotBase(r, s, hot || i == sel, e);
            if (hot)
                roundRect(r, 6.0f * s, Fade(WHITE, 0.10f * e));
            const Slot& cell = inv.slot(i);
            if (!cell.empty()) {
                drawBlockIcon(ctx.atlas, cell.block, inset(r, 7.0f * s), e);
                drawStackCount(r, cell.count, s, e);
            }
            if (i < kHotbarSlots) {
                textAt({r.x + 3.0f * s, r.y + 2.0f * s}, TextFormat("%d", i + 1), 10.0f * s,
                       Fade(i == sel ? kAccent : WHITE, (i == sel ? 0.9f : 0.30f) * e));
            }
        }

        // --- Tooltip --------------------------------------------------------
        if (ui.hovered >= 0 && ui.held.empty() && !inv.slot(ui.hovered).empty()) {
            const Slot& cell = inv.slot(ui.hovered);
            const BlockInfo& bi = blockInfo(cell.block);
            const float fs = 15.0f * s;
            const float sub = 12.0f * s;
            const char* line2 = TextFormat("x%d   %s", cell.count,
                                           bi.hardness < 0.0f ? "unbreakable" : "block");
            const float w = std::max(measure(bi.name, fs).x, measure(line2, sub).x) + 18.0f * s;
            const float h = fs + sub + 18.0f * s;
            Rectangle tip{ctx.mouse.x + 16.0f * s, ctx.mouse.y + 14.0f * s, w, h};
            // Keep the tooltip inside the viewport.
            tip.x = std::min(tip.x, vp.x + vp.width - w - 6.0f * s);
            tip.y = std::min(tip.y, vp.y + vp.height - h - 6.0f * s);
            panel(tip, s, kPanelMid, kAccentDeep, e);
            textAt({tip.x + 9.0f * s, tip.y + 7.0f * s}, bi.name, fs, Fade(pal::hudText, e));
            textAt({tip.x + 9.0f * s, tip.y + 7.0f * s + fs + 2.0f * s}, line2, sub,
                   Fade(pal::hudDim, e));
        }

        // --- Held stack rides the cursor ------------------------------------
        if (!ui.held.empty()) {
            const float is = L.slot * 0.82f;
            const Rectangle r{ctx.mouse.x - is * 0.5f, ctx.mouse.y - is * 0.5f, is, is};
            DrawCircleV(ctx.mouse, is * 0.62f, Fade(BLACK, 0.30f));
            drawBlockIcon(ctx.atlas, ui.held.block, r, 1.0f);
            drawStackCount(r, ui.held.count, s, 1.0f);
        }

        textCentered(pr.x + pr.width * 0.5f, pr.y + pr.height + 10.0f * s,
                     "LMB TAKE / PLACE      RMB SPLIT      E OR ESC TO CLOSE", 12.0f * s,
                     Fade(pal::hudDim, 0.9f * e));
    }

    // ----------------------------------------------------------- Title screen

    MenuAction drawTitleScreen(const HudContext& ctx, MenuState& st) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const float cx = vp.x + vp.width * 0.5f;

        // Backdrop: keep the orbiting world visible but push it back.
        DrawRectangleRec(vp, Fade(BLACK, 0.34f));
        DrawRectangleGradientEx(vp, Fade(BLACK, 0.55f), Fade(BLACK, 0.10f), Fade(BLACK, 0.55f),
                                Fade(BLACK, 0.10f));
        vignette(vp, BLACK, 0.38f);

        // --- Logo -----------------------------------------------------------
        const char* title = "VOXHAVEN";
        float ts = 78.0f * s;
        Vector2 ext = displayMeasure(title, ts);
        if (ext.x > vp.width * 0.74f) { // never let the logo run off a narrow window
            ts *= vp.width * 0.74f / ext.x;
            ext = displayMeasure(title, ts);
        }
        const float bob = std::sin(ctx.time * 1.2f) * 4.0f * s;
        const float lx = cx - ext.x * 0.5f;
        const float ly = vp.y + vp.height * 0.15f + bob;

        displayAt({lx + 7.0f * s, ly + 8.0f * s}, title, ts, Fade(BLACK, 0.55f)); // cast shadow
        displayAt({lx + 3.0f * s, ly + 3.0f * s}, title, ts, kAccentDeep);        // extrude
        displayAt({lx, ly}, title, ts, Color{255, 231, 148, 255});

        // Accent rule + subtitle.
        DrawRectangleGradientEx({lx, ly + ext.y + 6.0f * s, ext.x, std::max(2.0f, 4.0f * s)},
                                Fade(kAccent, 0.0f), Fade(kAccent, 0.0f), Fade(kAccent, 0.9f),
                                Fade(kAccent, 0.9f));
        DrawRectangleGradientEx({lx, ly + ext.y + 6.0f * s, ext.x * 0.5f, std::max(2.0f, 4.0f * s)},
                                Fade(kAccent, 0.15f), Fade(kAccent, 0.15f), Fade(kAccent, 0.9f),
                                Fade(kAccent, 0.9f));
        textCentered(cx, ly + ext.y + 16.0f * s, "A N   R L G E   V O X E L   S A N D B O X", 15.0f * s,
                     Fade(pal::hudDim, 0.95f));

        // --- Menu -------------------------------------------------------------
        constexpr std::array<MenuEntry, 3> entries = {{{"PLAY", MenuAction::Play},
                                                       {"SETTINGS", MenuAction::Settings},
                                                       {"QUIT", MenuAction::Quit}}};
        const float menuW = std::min(340.0f * s, vp.width * 0.6f);
        const Rectangle area{cx - menuW * 0.5f, vp.y + vp.height * 0.44f, menuW, 0.0f};
        const MenuAction fired = menuList(ctx, st, area, entries, 20.0f * s, s);

        // --- Controls legend ---------------------------------------------------
        struct Hint {
            const char* key;
            const char* what;
        };
        constexpr std::array<Hint, 12> hints = {{{"WASD", "MOVE"},
                                                 {"MOUSE", "LOOK"},
                                                 {"SPACE", "JUMP"},
                                                 {"SHIFT", "SPRINT"},
                                                 {"LMB", "MINE"},
                                                 {"RMB", "PLACE"},
                                                 {"1-9", "SELECT"},
                                                 {"WHEEL", "CYCLE"},
                                                 {"E", "INVENTORY"},
                                                 {"F", "FLY"},
                                                 {"T", "FAST TIME"},
                                                 {"ESC", "PAUSE"}}};
        const float legendW = std::min(vp.width * 0.88f, 900.0f * s);
        const float fs = 12.0f * s;
        const float rowH = fs * 2.2f;
        const Rectangle legend{cx - legendW * 0.5f, vp.y + vp.height - rowH * 3.0f - 54.0f * s, legendW,
                               rowH * 3.0f + 20.0f * s};
        panel(legend, s, Fade(kPanelDeep, 0.86f), kEdgeSoft);
        const float colW = (legend.width - 24.0f * s) / 4.0f;
        for (std::size_t i = 0; i < hints.size(); ++i) {
            const int col = static_cast<int>(i) % 4;
            const int row = static_cast<int>(i) / 4;
            const float x = legend.x + 12.0f * s + static_cast<float>(col) * colW;
            const float y = legend.y + 10.0f * s + static_cast<float>(row) * rowH;
            const float capW = measure(hints[i].key, fs).x + fs * 1.2f;
            drawKeyCap({x, y, capW, fs * 1.7f}, hints[i].key, fs);
            textAt({x + capW + 8.0f * s, y + fs * 0.32f}, hints[i].what, fs, Fade(pal::hudDim, 0.95f));
        }

        // --- Meta line ---------------------------------------------------------
        textAt({vp.x + 18.0f * s, vp.y + vp.height - 26.0f * s},
               TextFormat("%s   seed %u   %s", ctx.versionText, ctx.seed, ctx.worldName), 12.0f * s,
               Fade(pal::hudDim, 0.75f));
        textRight(vp.x + vp.width - 18.0f * s, vp.y + vp.height - 26.0f * s,
                  "ARROWS / MOUSE TO NAVIGATE   ENTER TO CONFIRM", 12.0f * s,
                  Fade(pal::hudDim, 0.75f));
        return fired;
    }

    // ------------------------------------------------------------- Pause menu

    MenuAction drawPauseMenu(const HudContext& ctx, MenuState& st) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const float cx = vp.x + vp.width * 0.5f;

        DrawRectangleRec(vp, Fade(BLACK, 0.60f));
        vignette(vp, BLACK, 0.35f);

        constexpr std::array<MenuEntry, 3> entries = {{{"RESUME", MenuAction::Resume},
                                                       {"SETTINGS", MenuAction::Settings},
                                                       {"SAVE AND QUIT", MenuAction::SaveAndQuit}}};
        const float fs = 19.0f * s;
        const float rowH = fs * 2.05f;
        const float gap = fs * 0.42f;
        const float pw = std::min(400.0f * s, vp.width * 0.86f);
        const float headH = 74.0f * s;
        const float ph = headH + rowH * 3.0f + gap * 2.0f + 46.0f * s;
        const Rectangle pr{cx - pw * 0.5f, vp.y + (vp.height - ph) * 0.5f, pw, ph};
        panel(pr, s, kPanelDeep, kEdge);

        const float hs = 40.0f * s;
        textCentered(cx, pr.y + 18.0f * s, "PAUSED", hs, pal::hudText);
        const float ruleW = pw * 0.42f;
        DrawRectangleRec({cx - ruleW * 0.5f, pr.y + 18.0f * s + hs + 6.0f * s, ruleW,
                          std::max(2.0f, 3.0f * s)},
                         Fade(kAccent, 0.85f));

        const Rectangle area{pr.x + 26.0f * s, pr.y + headH, pw - 52.0f * s, 0.0f};
        const MenuAction fired = menuList(ctx, st, area, entries, fs, s);

        textCentered(cx, pr.y + pr.height - 26.0f * s, "ESC TO RESUME", 12.0f * s,
                     Fade(pal::hudDim, 0.85f));
        return fired;
    }

    // --------------------------------------------------------- Settings panel

    namespace {

        enum class RowKind { SliderInt, SliderFloat, Toggle, Button };

        struct SettingRow {
            const char* label;
            RowKind kind;
            float lo;
            float hi;
            float* fval;
            int* ival;
            bool* bval;
            const char* fmt; // printf format applied to the display value
        };

    } // namespace

    MenuAction drawSettingsPanel(const HudContext& ctx, SettingsUiState& st) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const float cx = vp.x + vp.width * 0.5f;

        // Rows point straight at vox::settings, so edits are live with no apply.
        std::array<SettingRow, 9> rows = {{
            {"RENDER DISTANCE", RowKind::SliderInt, 2.0f, 12.0f, nullptr, &settings.viewRadius, nullptr,
             "%.0f chunks"},
            {"FIELD OF VIEW", RowKind::SliderFloat, 60.0f, 110.0f, &settings.fov, nullptr, nullptr,
             "%.0f deg"},
            {"MOUSE SENSITIVITY", RowKind::SliderFloat, 0.0008f, 0.0080f, &settings.mouseSensitivity,
             nullptr, nullptr, nullptr},
            {"MASTER VOLUME", RowKind::SliderFloat, 0.0f, 1.0f, &settings.masterVolume, nullptr, nullptr,
             nullptr},
            {"INVERT Y", RowKind::Toggle, 0, 0, nullptr, nullptr, &settings.invertY, nullptr},
            {"CREATIVE MODE", RowKind::Toggle, 0, 0, nullptr, nullptr, &settings.creative, nullptr},
            {"SMOOTH LIGHTING", RowKind::Toggle, 0, 0, nullptr, nullptr, &settings.smoothLighting,
             nullptr},
            {"SHOW DEBUG LINE", RowKind::Toggle, 0, 0, nullptr, nullptr, &settings.showDebugLine,
             nullptr},
            {"BACK", RowKind::Button, 0, 0, nullptr, nullptr, nullptr, nullptr},
        }};
        const int rowCount = static_cast<int>(rows.size());

        DrawRectangleRec(vp, Fade(BLACK, 0.70f));
        vignette(vp, BLACK, 0.35f);

        const float fs = 14.0f * s;
        const float rowH = 40.0f * s;
        const float pad = 22.0f * s;
        const float headH = 58.0f * s;
        const float pw = std::min(580.0f * s, vp.width * 0.94f);
        const float ph = headH + rowH * static_cast<float>(rowCount) + pad * 1.6f;
        const Rectangle pr{cx - pw * 0.5f, vp.y + (vp.height - ph) * 0.5f, pw, ph};
        panel(pr, s, kPanelDeep, kEdge);

        textShadow({pr.x + pad, pr.y + 16.0f * s}, "SETTINGS", 24.0f * s, pal::hudText);
        DrawRectangleRec({pr.x + pad, pr.y + headH - 12.0f * s, pw - pad * 2.0f, std::max(1.0f, 2.0f * s)},
                         Fade(kAccentDeep, 0.8f));

        // Control column geometry (shared by drawing and hit testing).
        const float ctrlX = pr.x + pw * 0.50f;
        const float ctrlW = pw - pad - (ctrlX - pr.x) - 74.0f * s; // leave room for the value text
        const float valueX = pr.x + pw - pad;

        // --- Keyboard navigation ---------------------------------------------
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            st.focus = (st.focus - 1 + rowCount) % rowCount;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            st.focus = (st.focus + 1) % rowCount;
        st.focus = std::clamp(st.focus, 0, rowCount - 1);

        // --- Mouse ------------------------------------------------------------
        st.hovered = -1;
        for (int i = 0; i < rowCount; ++i) {
            const Rectangle r{pr.x + pad * 0.5f, pr.y + headH + static_cast<float>(i) * rowH,
                              pw - pad, rowH};
            if (CheckCollisionPointRec(ctx.mouse, r))
                st.hovered = i;
        }
        if (!ctx.mouseDown)
            st.activeSlider = -1; // release ends any drag, even off-panel

        const auto rowValue01 = [&](const SettingRow& row) {
            if (row.kind == RowKind::SliderInt && row.ival != nullptr)
                return std::clamp((static_cast<float>(*row.ival) - row.lo) / (row.hi - row.lo), 0.0f,
                                  1.0f);
            if (row.kind == RowKind::SliderFloat && row.fval != nullptr)
                return std::clamp((*row.fval - row.lo) / (row.hi - row.lo), 0.0f, 1.0f);
            return 0.0f;
        };
        const auto setRow01 = [&](const SettingRow& row, const float v01) {
            const float v = row.lo + std::clamp(v01, 0.0f, 1.0f) * (row.hi - row.lo);
            if (row.kind == RowKind::SliderInt && row.ival != nullptr) {
                *row.ival = static_cast<int>(std::lround(v));
                // Streaming invariant: always keep a two-chunk unload margin.
                if (row.ival == &settings.viewRadius)
                    settings.unloadRadius = settings.viewRadius + 2;
            } else if (row.fval != nullptr) {
                *row.fval = v;
            }
        };
        const auto nudge = [&](const SettingRow& row, const int dir) {
            if (row.kind == RowKind::SliderInt && row.ival != nullptr) {
                *row.ival = static_cast<int>(std::lround(
                    std::clamp(static_cast<float>(*row.ival + dir), row.lo, row.hi)));
                if (row.ival == &settings.viewRadius)
                    settings.unloadRadius = settings.viewRadius + 2;
            } else if (row.kind == RowKind::SliderFloat) {
                setRow01(row, rowValue01(row) + static_cast<float>(dir) * 0.05f);
            }
        };

        MenuAction fired = MenuAction::None;
        if (IsKeyPressed(KEY_ESCAPE))
            fired = MenuAction::Back;

        for (int i = 0; i < rowCount; ++i) {
            SettingRow& row = rows[static_cast<std::size_t>(i)];
            const Rectangle rr{pr.x + pad * 0.5f, pr.y + headH + static_cast<float>(i) * rowH,
                               pw - pad, rowH};
            const bool focused = i == st.focus || i == st.hovered;
            const Rectangle track{ctrlX, rr.y + rowH * 0.5f - 4.0f * s, ctrlW, 8.0f * s};

            // Row background: only the focused row gets a plate, so the list
            // stays quiet until the player looks at something.
            if (focused)
                roundRect(rr, 6.0f * s, Fade(WHITE, 0.05f));
            if (i == st.focus)
                roundRect({rr.x + 2.0f * s, rr.y + rowH * 0.25f, 4.0f * s, rowH * 0.5f}, 2.0f * s,
                          Fade(kAccent, 0.9f));

            switch (row.kind) {
            case RowKind::SliderInt:
            case RowKind::SliderFloat: {
                // Grabbing anywhere on the row's control half starts a drag.
                const Rectangle grab{track.x - 8.0f * s, rr.y, track.width + 16.0f * s, rowH};
                if (ctx.mousePressed && CheckCollisionPointRec(ctx.mouse, grab))
                    st.activeSlider = i;
                if (st.activeSlider == i && ctx.mouseDown)
                    setRow01(row, (ctx.mouse.x - track.x) / std::max(1.0f, track.width));
                if (i == st.focus) {
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
                        nudge(row, -1);
                    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                        nudge(row, 1);
                }

                const float v01 = rowValue01(row);
                well(track, s);
                roundRect({track.x, track.y, track.width * v01, track.height}, track.height * 0.5f,
                          Fade(kAccent, 0.92f));
                const Vector2 knob{track.x + track.width * v01, track.y + track.height * 0.5f};
                DrawCircleV({knob.x + 1.0f, knob.y + 2.0f}, 8.0f * s, Fade(BLACK, 0.45f));
                DrawCircleV(knob, 8.0f * s, focused ? WHITE : Color{224, 228, 240, 255});
                DrawCircleV(knob, 3.5f * s, kAccentDeep);

                const char* vt = nullptr;
                if (row.kind == RowKind::SliderInt)
                    vt = TextFormat("%d", row.ival != nullptr ? *row.ival : 0);
                else if (row.fmt != nullptr)
                    vt = TextFormat(row.fmt, row.fval != nullptr ? *row.fval : 0.0f);
                else
                    vt = TextFormat("%.0f%%", v01 * 100.0f);
                textRight(valueX, rr.y + rowH * 0.5f - fs * 0.55f, vt, fs, pal::hudText);
                break;
            }
            case RowKind::Toggle: {
                const Rectangle sw{ctrlX, rr.y + rowH * 0.5f - 11.0f * s, 48.0f * s, 22.0f * s};
                const bool hit = ctx.mousePressed && CheckCollisionPointRec(ctx.mouse, rr);
                const bool key = i == st.focus && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                                                   IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT));
                if ((hit || key) && row.bval != nullptr)
                    *row.bval = !*row.bval;

                const bool nowOn = row.bval != nullptr && *row.bval;
                roundRect(sw, sw.height * 0.5f, nowOn ? Fade(kAccent, 0.85f) : Fade(kPanelWell, 0.9f));
                roundRectLines(sw, sw.height * 0.5f, std::max(1.0f, 1.5f * s),
                               Fade(nowOn ? kAccent : kEdgeSoft, 0.95f));
                const float kx = nowOn ? sw.x + sw.width - sw.height * 0.5f : sw.x + sw.height * 0.5f;
                DrawCircleV({kx, sw.y + sw.height * 0.5f}, sw.height * 0.36f,
                            nowOn ? Color{28, 24, 12, 255} : Color{188, 196, 216, 255});
                textRight(valueX, rr.y + rowH * 0.5f - fs * 0.55f, nowOn ? "ON" : "OFF", fs,
                          nowOn ? kAccent : Fade(pal::hudDim, 0.9f));
                break;
            }
            case RowKind::Button: {
                const Rectangle br{ctrlX, rr.y + rowH * 0.5f - 13.0f * s, 96.0f * s, 26.0f * s};
                const bool hit = ctx.mousePressed && CheckCollisionPointRec(ctx.mouse, rr);
                const bool key = i == st.focus && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
                if (hit || key)
                    fired = MenuAction::Back;
                roundRect(br, 6.0f * s, focused ? Fade(kAccent, 0.85f) : Fade(kPanelMid, 0.95f));
                roundRectLines(br, 6.0f * s, std::max(1.0f, 1.6f * s),
                               Fade(focused ? kAccent : kEdgeSoft, 0.95f));
                const Vector2 e = measure("BACK", fs);
                textAt({br.x + (br.width - e.x) * 0.5f, br.y + (br.height - e.y) * 0.5f}, "BACK", fs,
                       focused ? Color{24, 20, 10, 255} : pal::hudText);
                break;
            }
            }

            textShadow({rr.x + 14.0f * s, rr.y + rowH * 0.5f - fs * 0.55f}, row.label, fs,
                       focused ? pal::hudText : Fade(pal::hudDim, 0.95f));
        }

        textCentered(cx, pr.y + pr.height - 20.0f * s,
                     "DRAG SLIDERS   ARROWS TO ADJUST   ESC TO GO BACK", 11.0f * s,
                     Fade(pal::hudDim, 0.8f));

        if (fired != MenuAction::None)
            st.pending = fired;
        return fired;
    }

} // namespace vox
