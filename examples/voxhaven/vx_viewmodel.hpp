#pragma once
#include "raylib.h"

#include "vx_blocks.hpp"

namespace vox {

    // First-person view model: the player's arm and whatever it is holding,
    // drawn in the 3D lane but anchored to the camera rather than the world.
    //
    // Held blocks render as a small cube wearing their own terrain texture;
    // held items render as their icon extruded into a thin slab, which is how
    // a flat 2D sprite is given presence in a 3D scene.
    struct ViewModelState {
        Block held = Block::Air;
        float swing = 0.0f;       // 0..1 through one swing arc; 0 = at rest
        float bobPhase = 0.0f;    // walk cycle, from the player controller
        float bobAmount = 0.0f;   // 0..1 how much of the bob to apply
        float equipDrop = 0.0f;   // 0..1, item dips out of frame when swapped
        float light = 1.0f;       // scene brightness so the arm matches the world
    };

    // Call from inside a submit3D callback on the foreground layer. Clears the
    // depth buffer first so the arm can never be swallowed by a nearby wall.
    void drawViewModel(const Camera3D& cam, Texture2D atlas, const ViewModelState& vm);

} // namespace vox
