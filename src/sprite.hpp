#pragma once
#include <vector>

#include "component.hpp"
#include "entity.hpp"
#include "raylib.h"
#include "transformer.hpp"

namespace rlge {
    class SpriteAnim : public Component {
    public:
        SpriteAnim(Entity& e, Texture2D& tex, const int frameW, const int frameH) :
            Component(e), texture_(tex), fw_(frameW), fh_(frameH) {}

        void addFrame(const Rectangle& src, const float time) {
            frames_.push_back({src, time});
        }

        // single-row strip at given row
        void loadStrip(const int row, const int frameCount, const float timePerFrame) {
            frames_.clear();
            for (auto i = 0; i < frameCount; ++i) {
                const Rectangle r{
                    static_cast<float>(i * fw_),
                    static_cast<float>(row * fh_),
                    static_cast<float>(fw_),
                    static_cast<float>(fh_)
                };
                addFrame(r, timePerFrame);
            }
        }

        void update(float dt) override {
            if (frames_.size() <= 1)
                return;
            timer_ += dt;
            if (timer_ >= frames_[idx_].time) {
                timer_ = 0.0f;
                idx_ = (idx_ + 1) % frames_.size();
            }
        }

        void draw() override {
            if (frames_.empty())
                return;
            const auto* t = entity().get<rlge::Transform>();
            if (!t)
                return;

            const Frame& f = frames_[idx_];
            const Vector2 pos{t->position.x, t->position.y};
            const Vector2 origin{f.rect.width * 0.5f, f.rect.height * 0.5f};
            const Rectangle dest{pos.x - origin.x, pos.y - origin.y, f.rect.width, f.rect.height};
            const float rotation = t->rotation;

            auto& rq = entity().scene().engine().renderer();
            rq.submitWorld(
                pos.y,
                [this, f, dest, origin, rotation]() {
                    DrawTexturePro(texture_, f.rect, dest, origin, rotation, WHITE);
                });
        }

    private:
        struct Frame {
            Rectangle rect;
            float time;
        };

        Texture2D& texture_;
        int fw_;
        int fh_;

        std::vector<Frame> frames_;
        int idx_ = 0;
        float timer_ = 0.0f;
    };
}
