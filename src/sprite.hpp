#pragma once
#include <vector>

#include "component.hpp"
#include "raylib.h"
#include "transformer.hpp"

namespace rlge {
    class Entity;

    class Sprite : public Component {
    public:
        Sprite(Entity& e, Texture2D& tex, int frameW, int frameH);
        void draw() override;
    protected:
        Texture2D& texture_;
        int fw_;
        int fh_;
    };

    class SpriteAnim : public Sprite {
    public:
        SpriteAnim(Entity& e, Texture2D& tex, int frameW, int frameH);

        void addFrame(const Rectangle& src, float time);

        void loadStrip(int row, int frameCount, float timePerFrame);

        void update(float dt) override;
        void draw() override;

    private:
        struct Frame {
            Rectangle rect;
            float time;
        };

        std::vector<Frame> frames_;
        int idx_ = 0;
        float timer_ = 0.0f;
    };
}
