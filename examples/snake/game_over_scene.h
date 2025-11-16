#pragma once
#include "render_entity.hpp"
#include "scene.hpp"

namespace snake {
    class Overlay final : public rlge::RenderEntity {
    public:
        explicit Overlay(rlge::Scene& s, const int score) : RenderEntity(s), score_(score) {}

        void draw() override;
    private:
        int score_{0};
    };

    class GameOverScene final : public rlge::Scene {
    public:
        explicit GameOverScene(rlge::Runtime& r, const int score) :
            Scene(r), score_(score) {}

        ~GameOverScene() override;

    private:
        void enter() override;
        void exit() override;
        void update(float dt) override;

        int score_{0};
        Overlay* overlay_{nullptr};

        friend class GameScene;
    };
}
