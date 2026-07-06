#pragma once
#include "vx_world.hpp"

namespace vox {

    // Builds GPU meshes for chunks: visible faces only, directional shading and
    // vertex ambient occlusion baked into vertex colors. Opaque geometry and
    // water live in separate meshes so water can render in a later 3D layer.
    class Mesher {
    public:
        // Rebuilds meshes for up to `budget` dirty chunks near the player.
        // Returns how many chunks were remeshed.
        static int remeshDirty(World& world, Vector3 playerPos, int budget);

    private:
        static void buildChunk(World& world, Chunk& chunk);
    };

} // namespace vox
