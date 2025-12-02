#pragma once
#include "component.hpp"
#include "lighting_system.hpp"
#include "raylib.h"
#include "render_layer.hpp"

namespace rlge {
    class Entity;

    class LitSprite : public Component {
    public:
        // Constructor without LayerId (defaults to world layer)
        LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap, 
                  int frameW, int frameH, LightingSystem& lighting);

        // Constructor with LayerId
        LitSprite(Entity& e, Texture2D& diffuse, Texture2D& normalMap, 
                  int frameW, int frameH, LightingSystem& lighting, LayerId layer);

        void draw() override;

        // Set the render layer
        void setLayer(LayerId layer) { layer_ = layer; }
        [[nodiscard]] LayerId layer() const { return layer_; }

    protected:
        Texture2D& diffuse_;
        Texture2D& normalMap_;
        int fw_;
        int fh_;
        LayerId layer_;
        LightingSystem& lighting_;
    };

} // namespace rlge
