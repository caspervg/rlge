#pragma once
#include "component.hpp"
#include "raylib.h"
#include "render_layer.hpp"
#include "transformer.hpp"

namespace rlge {
    class Entity;

    // Utility describing a grid-based spritesheet texture.
    class SpriteSheet {
    public:
        SpriteSheet(Texture2D& tex, int tileW, int tileH);

        Texture2D& texture() const;

        int tileWidth() const;
        int tileHeight() const;
        int columns() const;
        int rows() const;

        // Get source rectangle for tile at (column, row).
        Rectangle tile(int col, int row) const;

    private:
        Texture2D& texture_;
        int tw_;
        int th_;
    };

    // Component that draws a single tile from a SpriteSheet.
    class SheetSprite : public Component {
    public:
        SheetSprite(Entity& e, SpriteSheet& sheet, int col, int row, LayerId layer = InvalidLayerId);

        void setTile(int col, int row);

        // Set the render layer
        void setLayer(LayerId layer) { layer_ = layer; }
        LayerId layer() const { return layer_; }

        void draw() override;

    private:
        SpriteSheet& sheet_;
        int col_;
        int row_;
        LayerId layer_;
    };
}

