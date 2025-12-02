#pragma once
#include "scene.hpp"
#include "lighting_system.hpp"

namespace rlge {

    class LitScene : public Scene {
    public:
        explicit LitScene(Runtime& r);
        ~LitScene() override;

        void enter() override;
        void draw() override;

        // Override to draw content not affected by lighting (debug overlays, etc.)
        virtual void drawUnlit() {}

        // Access to the lighting system
        LightingSystem& lighting() { return lighting_; }
        const LightingSystem& lighting() const { return lighting_; }

    protected:
        LightingSystem lighting_;
        RenderTexture2D sceneBuffer_{};
    };

} // namespace rlge
