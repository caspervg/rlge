#pragma once
#include "circle_collider.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"

namespace neon {
    class NsGame;
    class Enemy;

    class Bullet final : public rlge::RenderEntity {
    public:
        Bullet(rlge::Scene& scene, NsGame* game, Vector2 pos, Vector2 vel);

        void update(float dt) override;
        void draw() override;

        // Called from the arena's collision handler.
        void hitEnemy(Enemy& enemy);

        [[nodiscard]] Vector2 velocity() const { return vel_; }

    private:
        NsGame* game_;
        rlge::Transform* tr_ = nullptr;
        Vector2 vel_;
        float life_;
        bool spent_ = false;
    };

} // namespace neon
