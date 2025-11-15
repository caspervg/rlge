#include "sprite.hpp"

#include "engine.hpp"
#include "entity.hpp"
#include "scene.hpp"

namespace rlge {
    Sprite::Sprite(Entity& e, Texture2D& tex, const int frameW, const int frameH)
        : Component(e)
        , texture_(tex)
        , fw_(frameW)
        , fh_(frameH) {}

    void Sprite::draw() {
        const auto* t = entity().get<Transform>();
        if (!t)
            return;

        const Rectangle src{
            0.0f,
            0.0f,
            static_cast<float>(fw_),
            static_cast<float>(fh_)
        };

        const Vector2 pos{t->position.x, t->position.y};
        const Vector2 scale{t->scale.x, t->scale.y};
        const Vector2 size{src.width * scale.x, src.height * scale.y};
        const Vector2 origin{size.x * 0.5f, size.y * 0.5f};
        const Rectangle dest{pos.x, pos.y, size.x, size.y};
        const float rotation = t->rotation;

        auto& rq = entity().scene().engine().renderer();
        rq.submitWorld(
            pos.y,
            [this, src, dest, origin, rotation]() {
                DrawTexturePro(texture_, src, dest, origin, rotation, WHITE);
            });
    }

    SpriteAnim::SpriteAnim(Entity& e, Texture2D& tex, const int frameW, const int frameH)
        : Sprite(e, tex, frameW, frameH) {}

    void SpriteAnim::addFrame(const Rectangle& src, const float time) {
        frames_.push_back({src, time});
    }

    void SpriteAnim::loadStrip(const int row, const int frameCount, const float timePerFrame) {
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

    void SpriteAnim::update(const float dt) {
        if (frames_.size() <= 1)
            return;
        timer_ += dt;
        if (timer_ >= frames_[idx_].time) {
            timer_ = 0.0f;
            idx_ = (idx_ + 1) % frames_.size();
        }
    }

    void SpriteAnim::draw() {
        if (frames_.empty())
            return;
        const auto* t = entity().get<Transform>();
        if (!t)
            return;

        const Frame& f = frames_[idx_];
        const Vector2 pos{t->position.x, t->position.y};
        const Vector2 scale{t->scale.x, t->scale.y};
        const Vector2 size{f.rect.width * scale.x, f.rect.height * scale.y};
        const Vector2 origin{size.x * 0.5f, size.y * 0.5f};
        const Rectangle dest{pos.x - origin.x, pos.y - origin.y, size.x, size.y};
        const float rotation = t->rotation;

        auto& rq = entity().scene().engine().renderer();
        rq.submitWorld(
            pos.y,
            [this, f, dest, origin, rotation]() {
                DrawTexturePro(texture_, f.rect, dest, origin, rotation, WHITE);
            });
    }
}
