#pragma once
#include "vx_blocks.hpp"
#include "vx_world.hpp"

namespace vox {

    // Two-channel voxel lighting, both 0..15 and stored per chunk in Chunk::light.
    //
    //   sky   - seeded at 15 from the top of the world and carried straight down a
    //           column without loss while BlockInfo::lightOpacity is 0, then spread
    //           in every direction losing 1 per step (plus the target's opacity).
    //   block - BFS flood from every BlockInfo::lightEmission > 0 voxel, losing 1
    //           per step (plus the target's opacity).
    //
    // Light only ever flows into *loaded* chunks, and a chunk is only ever made
    // brighter by its neighbours. That makes the streaming case monotone: a chunk
    // lit before its neighbours existed is re-brightened (and re-meshed) when they
    // arrive, and the system converges without ping-ponging.
    namespace light {

        inline constexpr int kMax = 15;

        // Full recomputation of both channels for one chunk. Seeds from the sky,
        // from local emitters and from the four already-lit horizontal neighbours,
        // then floods; light that spills into a neighbour marks it for remeshing.
        void computeChunk(World& world, Chunk& chunk);

        // computeChunk(), but a no-op when the chunk's light is already valid.
        void ensureChunk(World& world, Chunk& chunk);

        // Same, addressed by chunk coordinates. Silently ignores unloaded chunks.
        void ensureChunkAt(World& world, ChunkKey key);

        // Scoped relight after a single block change. Runs the classic
        // remove-then-refill BFS pair over the affected neighbourhood only; light
        // is capped at 15 so the flood cannot travel further than 15 blocks
        // horizontally (the sky column below the edit is the one unbounded axis).
        void updateBlock(World& world, int x, int y, int z, Block before, Block after);

    } // namespace light
} // namespace vox
