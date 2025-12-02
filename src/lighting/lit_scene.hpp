#pragma once
#include "lighting_system.hpp"
#include "scene.hpp"

namespace rlge {

    class LitScene : public Scene {
    public:
        explicit LitScene(Runtime& r);
        ~LitScene() override;

        void enter() override;
        RenderTexture2D* beginWorldRenderTarget() override;
        void afterWorldRender(RenderTexture2D* target, const std::vector<View>& views) override;

        // Override to draw content not affected by lighting (debug overlays, etc.)
        virtual void drawUnlit() {}

        // Access to the lighting system
        LightingSystem& lighting() { return lighting_; }
        [[nodiscard]] const LightingSystem& lighting() const { return lighting_; }

    protected:
        LightingSystem lighting_;
        RenderTexture2D sceneBuffer_{};
        Vector2 bufferSize_{0.0f, 0.0f};

    private:
        void ensureBuffersMatchWindow_();
    };

} // namespace rlge
