#pragma once
#include "raylib.h"

// VOXHAVEN - global tuning for the voxel sandbox.
namespace vox {

    struct Config {
        // Window
        float screenWidth = 1280.0f;
        float screenHeight = 720.0f;

        // World shape
        int chunkSize = 16;    // blocks per chunk edge (x/z)
        int worldHeight = 96;  // blocks per column (y)
        int seaLevel = 30;

        // Streaming
        int viewRadius = 6;        // chunks kept meshed around the player
        int unloadRadius = 8;      // chunks beyond this are dropped
        int genPerFrame = 3;       // generation budget per frame
        int meshPerFrame = 3;      // meshing budget per frame

        // Player physics (units = blocks)
        float playerWidth = 0.6f;
        float playerHeight = 1.8f;
        float eyeHeight = 1.62f;
        float walkSpeed = 4.4f;
        float sprintSpeed = 6.4f;
        float flySpeed = 12.0f;
        float swimSpeed = 2.6f;
        float gravity = 26.0f;
        float jumpVelocity = 8.6f;
        float mouseSensitivity = 0.0032f;

        // Interaction
        float reach = 6.0f;
        float placeRepeat = 0.22f;

        // Day cycle
        float dayLengthSeconds = 480.0f; // full day/night loop
        float nightLight = 0.16f;        // ambient floor at midnight

        // Fog (blocks)
        float fogStart = 58.0f;
        float fogEnd = 92.0f;

        // Autosave
        float autosaveSeconds = 25.0f;
    };

    inline const Config cfg{};

    namespace pal {
        inline constexpr Color hudText{235, 238, 245, 255};
        inline constexpr Color hudDim{160, 168, 190, 255};
        inline constexpr Color hudAccent{255, 214, 92, 255};
        inline constexpr Color skyDay{116, 172, 255, 255};
        inline constexpr Color skyDusk{255, 138, 90, 255};
        inline constexpr Color skyNight{10, 12, 32, 255};
        inline constexpr Color waterFog{22, 52, 110, 255};
    }

} // namespace vox
