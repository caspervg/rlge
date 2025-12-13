#include "sprite.hpp"

#include "entity.hpp"
#include "render_queue.hpp"
#include "scene.hpp"
#include "shader_effect.hpp"

namespace rlge {
    Sprite::Sprite(Entity& e, Texture2D& tex, const int frameW, const int frameH)
        : Component(e)
        , texture_(tex)
        , fw_(frameW)
        , fh_(frameH)
        , layer_(InvalidLayerId) {}

    Sprite::Sprite(Entity& e, Texture2D& tex, const int frameW, const int frameH, LayerId layer)
        : Component(e)
        , texture_(tex)
        , fw_(frameW)
        , fh_(frameH)
        , layer_(layer) {}

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

        auto& scene = entity().scene();
        auto& rq = scene.rq();

        // Resolve layer: use provided layer or default to world
        LayerId effectiveLayer = layer_;
        if (effectiveLayer == InvalidLayerId) {
            effectiveLayer = scene.layers().world();
        }

        // Check for per-entity shader effect
        auto* shaderEffect = entity().get<HasShaderEffect>();
        if (shaderEffect) {
            // Use custom draw command with shader (bypasses batching)
            Shader shader = shaderEffect->shader();
            rq.submitCustom(effectiveLayer, pos.y, shader, [this, &rq, src, dest, origin, rotation, shaderEffect]() {
                shaderEffect->apply();
                DrawTexturePro(texture_, src, dest, origin, rotation, WHITE);
            });
        } else {
            // Use batched sprite submission
            rq.submitSprite(effectiveLayer, pos.y, texture_,
                           src, dest, origin, rotation, WHITE);
        }
    }

    SpriteAnim::SpriteAnim(Entity& e, Texture2D& tex, const int frameW, const int frameH)
        : Sprite(e, tex, frameW, frameH) {}

    SpriteAnim::SpriteAnim(Entity& e, Texture2D& tex, const int frameW, const int frameH, LayerId layer)
        : Sprite(e, tex, frameW, frameH, layer) {}

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
        const Rectangle dest{pos.x, pos.y, size.x, size.y};
        const float rotation = t->rotation;

        auto& scene = entity().scene();
        auto& rq = scene.rq();

        // Resolve layer: use provided layer or default to world
        LayerId effectiveLayer = layer_;
        if (effectiveLayer == InvalidLayerId) {
            effectiveLayer = scene.layers().world();
        }

        // Check for per-entity shader effect
        auto* shaderEffect = entity().get<HasShaderEffect>();
        if (shaderEffect) {
            // Use custom draw command with shader (bypasses batching)
            Shader shader = shaderEffect->shader();
            rq.submitCustom(effectiveLayer, pos.y, shader, [this, &f, dest, origin, rotation, shaderEffect]() {
                shaderEffect->apply();
                DrawTexturePro(texture_, f.rect, dest, origin, rotation, WHITE);
            });
        } else {
            // Use batched sprite submission
            rq.submitSprite(effectiveLayer, pos.y, texture_,
                           f.rect, dest, origin, rotation, WHITE);
        }
    }
}
