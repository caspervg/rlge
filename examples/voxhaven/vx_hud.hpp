#pragma once
#include "raylib.h"

#include "vx_blocks.hpp"
#include "vx_inventory.hpp"

// VOXHAVEN - all 2D chrome: in-game HUD, inventory screen, title/pause menus
// and the live settings panel.
//
// Everything here is pure immediate-mode drawing plus the input handling that
// belongs to a widget. Nothing owns game state: the caller passes a HudContext
// snapshot each frame and keeps the small UI-local structs below alive between
// frames. No BeginDrawing/EndDrawing, no render targets - call these from
// inside the scene's queued UI callback.
//
// Layout is driven entirely off HudContext::viewport, scaled by a factor
// derived from it, so the whole UI tracks a resizable window.
namespace vox {

    // ---------------------------------------------------------------- Context

    // Per-frame snapshot the scene fills in. It is copied into the UI callback
    // by value, so the pointer/`const char*` members must outlive the frame
    // (scene members and string literals both qualify).
    struct HudContext {
        Rectangle viewport{};              // where to draw, in screen pixels
        const Inventory* inventory = nullptr;
        Texture2D atlas{};                 // block atlas, sampled via tileUV()
        float time = 0.0f;                 // seconds since scene start, for animation
        float dayFraction = 0.0f;          // 0..1 raw day clock; 0 = sunrise (scene's dayTime_)
        int fps = 0;
        Vector3 playerPos{};
        const char* biomeName = nullptr;   // nullptr / "" hides the biome chip
        bool underwater = false;
        bool flying = false;
        bool creative = false;
        bool sprinting = false;
        bool onGround = true;

        int health = 20;                   // hearts row is only drawn when showHealth
        int maxHealth = 20;
        bool showHealth = false;

        float breakProgress = 0.0f;        // 0..1, > 0 while mining
        bool hasTarget = false;            // a block is under the crosshair
        const char* lookingAtName = nullptr; // nullptr when not targeting a block

        int chunksLoaded = 0;
        int chunksDrawn = 0;
        int meshedThisFrame = 0;

        // Ring buffer for the FPS sparkline; head is the index that will be
        // written next. Leave all-zero to hide the graph.
        float fpsGraphSamples[64]{};
        int fpsGraphHead = 0;

        // ctx.time at which the hotbar selection last changed. The floating
        // block name fades out ~2.2s later; set it far in the past (e.g. -100)
        // to keep the name hidden.
        float selectionChangedAt = -100.0f;

        // Title screen / debug metadata.
        const char* versionText = "v0.1";
        const char* worldName = "voxhaven.world";
        unsigned int seed = 0;

        // Mouse state, forwarded by the scene. fillMouseFromRaylib() below does
        // the obvious thing if the scene has no reason to fake it.
        Vector2 mouse{-1.0f, -1.0f};
        bool mouseDown = false;            // left button held this frame
        bool mousePressed = false;         // left button went down this frame
        bool mouseReleased = false;        // left button came up this frame
        bool mouseRightPressed = false;
        float mouseWheel = 0.0f;
    };

    // Convenience: fill the six mouse fields from raylib's current input state.
    void fillMouseFromRaylib(HudContext& ctx);

    // --------------------------------------------------------------- Menus

    enum class MenuAction {
        None = 0,
        Play,
        Settings,
        Quit,
        Resume,
        SaveAndQuit,
        Back
    };

    // State for any keyboard+mouse navigable entry list. One instance per menu.
    struct MenuState {
        int index = 0;                     // keyboard highlight
        MenuAction pending = MenuAction::None;
        float openTime = 0.0f;             // ctx.time when the menu appeared (pop-in)
        Vector2 lastMouse{-1.0f, -1.0f};   // used to tell "mouse moved" from "mouse resting"
        bool mouseOwnsHighlight = false;

        void open(const float now) {
            openTime = now;
            pending = MenuAction::None;
        }
        // Read-and-clear. Call from the scene's update(): the draw callback runs
        // during the render flush, so the action is consumed the next frame.
        [[nodiscard]] MenuAction take() {
            const MenuAction a = pending;
            pending = MenuAction::None;
            return a;
        }
    };

    // State for the settings panel (sliders need drag tracking).
    struct SettingsUiState {
        int focus = 0;                     // keyboard-focused row
        int activeSlider = -1;             // row currently being dragged, -1 = none
        int hovered = -1;
        MenuAction pending = MenuAction::None;

        [[nodiscard]] MenuAction take() {
            const MenuAction a = pending;
            pending = MenuAction::None;
            return a;
        }
    };

    // State for the inventory overlay.
    struct InventoryUiState {
        Slot held{};                       // stack following the cursor
        int hovered = -1;                  // slot index under the cursor, -1 = none
        float openTime = 0.0f;             // ctx.time when opened (pop-in animation)

        void open(const float now) {
            openTime = now;
            hovered = -1;
        }
    };

    // ---------------------------------------------------------------- Draws

    // In-game overlay: vignette, underwater tint, crosshair + break ring,
    // hotbar, status chips, clock dial and the optional debug strip.
    void drawHud(const HudContext& ctx);

    // Full inventory overlay. Handles hover, click-to-pick-up / click-to-place
    // and right-click splitting itself, mutating `inv` and `ui` directly.
    void drawInventoryScreen(const HudContext& ctx, Inventory& inv, InventoryUiState& ui);

    // Put any cursor-held stack back into the grid. Call when the inventory
    // closes so a held stack cannot be lost.
    void returnHeldToInventory(Inventory& inv, InventoryUiState& ui);

    // Title screen: logo, Play / Settings / Quit, version+seed line, controls.
    // Also writes the result into `st.pending` so it can be consumed a frame
    // later when drawing happens inside a queued callback.
    MenuAction drawTitleScreen(const HudContext& ctx, MenuState& st);

    // Pause overlay: Resume / Settings / Save and Quit.
    MenuAction drawPauseMenu(const HudContext& ctx, MenuState& st);

    // Live settings panel. Sliders and toggles write straight into
    // vox::settings; returns MenuAction::Back when the player leaves.
    MenuAction drawSettingsPanel(const HudContext& ctx, SettingsUiState& st);

    // ------------------------------------------------------------- Helpers
    // Exposed because the scene may want to reuse the exact HUD text look.

    // Installs the UI and display typefaces. Passing a zero-id Font falls back
    // to raylib's built-in font, so the game still runs without the .ttf files.
    void setHudFonts(Font ui, Font display);

    void hudText(Vector2 at, const char* text, float size, Color color);
    void hudTextShadow(Vector2 at, const char* text, float size, Color color);
    [[nodiscard]] Vector2 hudMeasure(const char* text, float size);

    // Scale factor the whole UI is built from (1.0 at 1280x720).
    [[nodiscard]] float hudScale(Rectangle viewport);

    // Draw a block as a chunky pseudo-3D icon inside `dst`.
    void drawBlockIcon(Texture2D atlas, Block b, Rectangle dst, float alpha = 1.0f);

} // namespace vox
