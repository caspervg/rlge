#pragma once
#include "circle_collider.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"

#include "ns_events.hpp"

namespace neon {
    class NsGame;
    class ArenaScene;

    // One class, five behaviors. Enemies are trigger colliders on LAYER_ENEMY;
    // damage/death is driven from the arena's collision response handler.
    class Enemy final : public rlge::RenderEntity {
    public:
        Enemy(rlge::Scene& scene, NsGame* game, EnemyKind kind, Vector2 pos, int wave);

        void update(float dt) override;
        void draw() override;

        // Called from collision handlers (safe spawn context).
        void takeDamage(int dmg, Vector2 hitDir);
        void contactPlayer(); // dies without awarding score

        [[nodiscard]] bool alive() const { return !dead_; }
        [[nodiscard]] EnemyKind kind() const { return kind_; }
        [[nodiscard]] float radius() const { return radius_; }
        [[nodiscard]] Vector2 pos() const;

    private:
        ArenaScene& arena();
        void die_(bool awardScore, Vector2 hitDir);
        void behave_(float dt, Vector2 playerPos);

        NsGame* game_;
        EnemyKind kind_;
        rlge::Transform* tr_ = nullptr;
        rlge::CircleCollider* col_ = nullptr;

        int hp_ = 1;
        int score_ = 100;
        float radius_ = 14.0f;
        float speed_ = 150.0f;
        Color color_ = WHITE;
        int sides_ = 3;

        Vector2 vel_{0.0f, 0.0f};
        float age_ = 0.0f;
        float spawnT_ = 0.0f;     // 0..1 scale-in
        float flash_ = 0.0f;      // white flash after taking a hit
        float wobblePhase_ = 0.0f;
        float spin_ = 0.0f;
        bool dead_ = false;
    };

} // namespace neon
