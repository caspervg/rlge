#include "sprite_sheet.hpp"

#include <cmath>

#include "entity.hpp"
#include "scene.hpp"
#include "shader_effect.hpp"

namespace rlge {
    SpriteSheet::SpriteSheet(Texture2D& tex, const int tileW, const int tileH)
        : texture_(tex)
        , tw_(tileW)
        , th_(tileH) {}

    Texture2D& SpriteSheet::texture() const {
        return texture_;
    }

    int SpriteSheet::tileWidth() const {
        return tw_;
    }

    int SpriteSheet::tileHeight() const {
        return th_;
    }

    int SpriteSheet::columns() const {
        return tw_ > 0 ? texture_.width / tw_ : 0;
    }

    int SpriteSheet::rows() const {
        return th_ > 0 ? texture_.height / th_ : 0;
    }

    Rectangle SpriteSheet::tile(const int col, const int row) const {
        return Rectangle{
            static_cast<float>(col * tw_),
            static_cast<float>(row * th_),
            static_cast<float>(tw_),
            static_cast<float>(th_)
        };
    }

    SheetSprite::SheetSprite(Entity& e, SpriteSheet& sheet, const int col, const int row, LayerId layer)
        : Component(e)
        , sheet_(sheet)
        , col_(col)
        , row_(row)
        , layer_(layer) {}

    void SheetSprite::setTile(const int col, const int row) {
        col_ = col;
        row_ = row;
    }

    void SheetSprite::draw() {
        const auto* t = entity().get<Transform>();
        if (!t)
            return;

        const Rectangle src = sheet_.tile(col_, row_);
        const Vector2 pos{t->position.x, t->position.y};
        const Vector2 scale{t->scale.x, t->scale.y};
        const Vector2 size{src.width * scale.x, src.height * scale.y};
        const Vector2 origin{size.x * 0.5f, size.y * 0.5f};
        Rectangle dest{pos.x, pos.y, size.x, size.y};

        // Snap to integer pixels to avoid subpixel artifacts.
        dest.x = std::roundf(dest.x);
        dest.y = std::roundf(dest.y);
        const float rotation = t->rotation;

        auto& scene = entity().scene();
        auto& rq = scene.rq();

        // Resolve layer: use provided layer or default to world
        LayerId effectiveLayer = layer_;
        if (effectiveLayer == InvalidLayerId) {
            effectiveLayer = scene.layers().world();
        }

        // Check for per-entity shader effect
        auto* shaderEffect = entity().get<IShaderEffect>();
        if (shaderEffect) {
            // Use custom draw command with shader (bypasses batching)
            Shader shader = shaderEffect->shader();
            rq.submitCustom(effectiveLayer, pos.y, shader, [this, src, dest, origin, rotation, shaderEffect]() {
                shaderEffect->apply();
                DrawTexturePro(sheet_.texture(), src, dest, origin, rotation, WHITE);
            });
        } else {
            // Use batched sprite submission
            rq.submitSprite(effectiveLayer, pos.y, sheet_.texture(),
                           src, dest, origin, rotation, WHITE);
        }
    }
}
