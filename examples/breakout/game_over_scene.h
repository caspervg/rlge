#pragma once
#include "render_entity.hpp"
#include "scene.hpp"

namespace breakout {
    using namespace rlge;

    class Overlay final : public RenderEntity {
    public:
        explicit Overlay(Scene& s, const int score) : RenderEntity(s), score_(score) {}

        void draw() override;
    private:
        int score_{0};
    };

    class GameOverScene final : public Scene {
    public:
        explicit GameOverScene(Runtime& r, const int score) :
            Scene(r), score_(score) {}

        ~GameOverScene() override;

    private:
        void enter() override;
        void exit() override;
        void update(float dt) override;

        int score_{0};
        Overlay* overlay_{nullptr};
    };
}
