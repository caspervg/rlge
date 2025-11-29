#pragma once
#include <vector>

#include "component.hpp"
#include "raylib.h"
#include "render_layer.hpp"
#include "transformer.hpp"

namespace rlge {
    class Entity;

    class Sprite : public Component {
    public:
        // Constructor without LayerId (defaults to world layer)
        Sprite(Entity& e, Texture2D& tex, int frameW, int frameH);

        // Constructor with LayerId
        Sprite(Entity& e, Texture2D& tex, int frameW, int frameH, LayerId layer);

        void draw() override;

        // Set the render layer
        void setLayer(LayerId layer) { layer_ = layer; }
        LayerId layer() const { return layer_; }

    protected:
        Texture2D& texture_;
        int fw_;
        int fh_;
        LayerId layer_;
    };

    class SpriteAnim : public Sprite {
    public:
        // Constructor without LayerId (defaults to world layer)
        SpriteAnim(Entity& e, Texture2D& tex, int frameW, int frameH);

        // Constructor with LayerId
        SpriteAnim(Entity& e, Texture2D& tex, int frameW, int frameH, LayerId layer);

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
