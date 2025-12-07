#pragma once
#include "box2d_physics.hpp"
#include "powerup_types.hpp"
#include "render_entity.hpp"

namespace breakout {
    using namespace rlge;

    class PowerUp final : public RenderEntity {
    public:
        PowerUp(Scene& s, PowerUpType type, float x, float y);

        void update(float dt) override;
        void draw() override;

        [[nodiscard]] PowerUpType type() const { return type_; }
        [[nodiscard]] bool isCollected() const { return collected_; }

        void collect();

    private:
        PowerUpType type_;
        PowerUpConfig config_;
        Box2DBody* body_{nullptr};
        bool collected_{false};
        float fallSpeed_{120.0f};

        float bobTime_{0.0f};
        float bobAmplitude_{3.0f};
    };

} // namespace breakout
