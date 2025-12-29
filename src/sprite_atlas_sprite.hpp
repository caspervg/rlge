#pragma once

#include "sprite.hpp"
#include "sprite_atlas.hpp"

namespace rlge {
    template <typename StateEnum>
    void SpriteAtlas<StateEnum>::addTo(SpriteAnim& sprite) const {
        for (const auto& clip : clips_) {
            for (auto i = 0; i < clip.frameCount; ++i) {
                const int frameIndex = clip.startFrame + i;
                sprite.addFrame(frames_.at(static_cast<size_t>(frameIndex)), clip.frameTime);
            }
        }
    }
} // namespace rlge
