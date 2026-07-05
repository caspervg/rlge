#pragma once
#include "circle_collider.hpp"
#include "raylib.h"
#include "render_entity.hpp"
#include "transformer.hpp"

#include "ns_events.hpp"

namespace neon {
    class NsGame;
    class Player;
    class ArenaScene;

    class Pickup final : public rlge::RenderEntity {
    public:
        Pickup(rlge::Scene& scene, NsGame* game, PickupType type, Vector2 pos);

        void update(float dt) override;
        void draw() override;

        // Called from the arena's collision handler.
        void collect(Player& player);

        [[nodiscard]] PickupType type() const { return type_; }

    private:
        ArenaScene& arena();
        [[nodiscard]] Color color() const;

        NsGame* game_;
        PickupType type_;
        rlge::Transform* tr_ = nullptr;
        Vector2 vel_{0.0f, 0.0f};
        float age_ = 0.0f;
        float life_ = 11.0f;
        bool collected_ = false;
    };

} // namespace neon
