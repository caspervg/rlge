#pragma once
#include "raylib.h"

#include "asset.hpp"

namespace vox {

    // Full-screen post-processing chain, driven through RLGE's
    // Scene::beginWorldRenderTarget() / afterWorldRender() hooks: the world is
    // rendered into an offscreen target, the chain runs over it, and the HUD is
    // drawn afterwards straight to the backbuffer so it stays crisp.
    //
    //   world -> [bright pass] -> [blur H] -> [blur V] -┐
    //         └------------------------------> [composite] -> screen
    //
    // Every stage is individually switchable from the settings panel; with all
    // of them off the chain collapses to a single blit.
    class PostFx {
    public:
        void init(rlge::AssetStore& assets);
        void shutdown();

        // Returns the world render target, resizing it to the viewport if the
        // window changed. Returns nullptr when post-processing is disabled, and
        // the engine then renders straight to the backbuffer.
        RenderTexture2D* target(int width, int height);

        // Composites the target back to the screen with the enabled effects.
        void apply(Rectangle viewport, float time, bool underwater, float dayFactor);

        [[nodiscard]] bool enabled() const;

    private:
        void ensureTargets_(int width, int height);
        void releaseTargets_();

        RenderTexture2D scene_{};
        RenderTexture2D bloomA_{};   // half resolution ping/pong
        RenderTexture2D bloomB_{};
        int width_ = 0;
        int height_ = 0;
        bool ready_ = false;

        Shader bright_{};
        Shader blur_{};
        Shader composite_{};

        int locBrightThreshold_ = -1;
        int locBlurDir_ = -1;
        int locBlurRes_ = -1;
        int locCompBloomTex_ = -1;
        int locCompBloom_ = -1;
        int locCompTime_ = -1;
        int locCompRes_ = -1;
        int locCompGrade_ = -1;      // vec3(contrast, saturation, brightness)
        int locCompFlags_ = -1;      // vec4(vignette, grain, aberration, underwater)
    };

} // namespace vox
