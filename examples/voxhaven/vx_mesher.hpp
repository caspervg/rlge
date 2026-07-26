#pragma once
#include "vx_world.hpp"

namespace vox {

    // Builds GPU meshes for chunks: visible faces only, directional shading,
    // vertex ambient occlusion and both light channels baked into vertex colors.
    // Opaque geometry and water live in separate meshes so water can render in a
    // later 3D layer.
    //
    // Vertex color contract (the chunk fragment shader depends on it):
    //   R = ambient occlusion * face shade      (static, baked)
    //   G = skylight  / 15                      (scaled by the day/night factor)
    //   B = blocklight / 15                     (constant through the day)
    //   A = 255 opaque, 170 water
    // so the shader can do `light = max(G * dayFactor, B) * R` and the day cycle
    // never needs a remesh.
    //
    // Vertex attributes:
    //   texcoord0 (vertexTexCoord)  atlas UV; on a merged quad it runs past the
    //                               tile and must be wrapped by the shader.
    //   texcoord2 (vertexTexCoord2) the quad's tile origin in atlas space, so the
    //                               shader can do that wrap:
    //     const vec2 kTile = vec2(1.0/8.0, 1.0/4.0);
    //     vec2 uv = tileOrigin + mod(fragTexCoord - tileOrigin, kTile);
    class Mesher {
    public:
        // Rebuilds meshes for up to `budget` dirty chunks near the player.
        // Returns how many chunks were remeshed.
        static int remeshDirty(World& world, Vector3 playerPos, int budget);

        // Coplanar faces with identical tile, AO and light are merged into one
        // quad. Turn this off to get exactly one quad per block face (needed if
        // the fragment shader does not wrap texcoord0 back into its tile).
        inline static bool greedy = true;

    private:
        static void buildChunk(World& world, Chunk& chunk);
    };

} // namespace vox
