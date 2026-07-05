#pragma once
#include "camera.hpp"
#include "scene.hpp"

namespace neon {
    class NsGame;

    class MenuScene final : public rlge::Scene {
    public:
        MenuScene(rlge::Runtime& r, NsGame* game);

        void enter() override;
        void update(float dt) override;
        void draw() override;

    private:
        void fitCamera_();

        NsGame* game_;
        rlge::Camera2DController camera_;
        float time_ = 0.0f;
    };

} // namespace neon
