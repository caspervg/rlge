#pragma once
#include "raylib.h"

// VOXHAVEN - global tuning for the voxel sandbox.
namespace vox {

    // Compile-time constants: world shape and physics that never change at runtime.
    struct Config {
        // Window
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;

        // World shape
        int chunkSize = 16;    // blocks per chunk edge (x/z)
        int worldHeight = 96;  // blocks per column (y)
        int seaLevel = 30;

        // Streaming budgets (per frame)
        int genPerFrame = 2;
        int meshPerFrame = 2;
        int lightPerFrame = 2;

        // Player physics (units = blocks)
        float playerWidth = 0.6f;
        float playerHeight = 1.8f;
        float eyeHeight = 1.62f;
        float walkSpeed = 4.4f;
        float sprintSpeed = 6.6f;
        float flySpeed = 14.0f;
        float swimSpeed = 2.6f;
        float gravity = 26.0f;
        float jumpVelocity = 8.6f;
        float stepHeight = 0.6f;  // auto-step up single blocks

        // Interaction
        float reach = 6.0f;
        float placeRepeat = 0.20f;

        // Lighting
        int maxLight = 15;
        float nightLight = 0.10f;  // ambient floor at midnight

        // Day cycle
        float dayLengthSeconds = 600.0f;

        // Autosave
        float autosaveSeconds = 25.0f;
    };

    inline const Config cfg{};

    // Runtime-tunable settings, driven by the in-game settings panel.
    struct Settings {
        int viewRadius = 6;         // chunks meshed around the player
        int unloadRadius = 8;       // chunks beyond this are dropped
        float fov = 72.0f;
        float mouseSensitivity = 0.0032f;
        bool invertY = false;
        bool creative = false;      // infinite blocks, instant mining, fly
        bool showDebugLine = true;
        bool smoothLighting = true;
        float masterVolume = 0.8f;

        [[nodiscard]] float fogStart() const { return static_cast<float>(viewRadius) * 16.0f * 0.55f; }
        [[nodiscard]] float fogEnd() const { return static_cast<float>(viewRadius) * 16.0f * 0.95f; }
    };

    inline Settings settings{};

    namespace pal {
        inline constexpr Color hudText{235, 238, 245, 255};
        inline constexpr Color hudDim{160, 168, 190, 255};
        inline constexpr Color hudAccent{255, 214, 92, 255};
        inline constexpr Color hudPanel{18, 20, 30, 232};
        inline constexpr Color hudSlot{44, 48, 66, 235};
        inline constexpr Color skyDay{116, 172, 255, 255};
        inline constexpr Color skyDusk{255, 138, 90, 255};
        inline constexpr Color skyNight{8, 10, 28, 255};
        inline constexpr Color waterFog{22, 52, 110, 255};
    }

} // namespace vox
