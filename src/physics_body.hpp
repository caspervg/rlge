#pragma once
#include "component.hpp"
#include "raylib.h"
#include "collision/collider.hpp"

namespace rlge {
    struct CollisionEvent;

    enum class BodyType {
        Dynamic,
        Kinematic
    };

    struct GroundInfo {
        bool grounded = false;
        Vector2 normal{0.0f, 1.0f};
        Collider* collider = nullptr;
    };

    struct PhysicsBodyConfig {
        float mass = 1.0f;
        Vector2 velocity{0.0f, 0.0f};
        Vector2 gravity{0.0f, 981.0f};    // pixels per second squared
        float linearDamping = 0.05f;
        float maxSpeed = 0.0f;                  // 0.0 = unlimited
        float restitution = 1.0f;               // 1.0 = perfectly elastic
        float friction = 0.0f;                  // 0.0 = no tangential energy loss
        BodyType type = BodyType::Dynamic;
    };

    class PhysicsBody final : public Component {
    public:
        explicit PhysicsBody(Entity& e, const PhysicsBodyConfig& cfg = {})
            : Component(e)
            , gravity_(cfg.gravity)
            , velocity_(cfg.velocity)
            , linearDamping_(cfg.linearDamping)
            , mass_(cfg.mass)
            , maxSpeed_(cfg.maxSpeed)
            , restitution_(cfg.restitution)
            , friction_(cfg.friction)
            , bodyType_(cfg.type) {}

        const GroundInfo& groundInfo() const;
        [[nodiscard]] bool isGrounded() const;

        void reflectOffNormal(const Vector2& normal);
        void applyForce(const Vector2& force);

        void onCollision(const CollisionEvent& event);

        void update(float dt) override;

        // Getters
        [[nodiscard]] Vector2 acceleration() const { return acceleration_; }
        [[nodiscard]] float angularVelocity() const { return angularVelocity_; }
        [[nodiscard]] Vector2 velocity() const { return velocity_; }
        [[nodiscard]] float mass() const { return mass_; }
        [[nodiscard]] float restitution() const { return restitution_; }
        [[nodiscard]] float friction() const { return friction_; }
        [[nodiscard]] BodyType type() const { return bodyType_; }

        // Setters
        void setAcceleration(const Vector2& acc) { acceleration_ = acc; }
        void setAngularVelocity(const float omega) { angularVelocity_ = omega; }
        void setVelocity(const Vector2& vel) { velocity_ = vel; }
        void setMass(const float m) { mass_ = m; }
        void setRestitution(const float e) { restitution_ = e; }
        void setFriction(const float f) { friction_ = f; }
        void setType(const BodyType t) { bodyType_ = t; }

    private:
        // State
        Vector2 acceleration_ = {0.0f, 0.0f};
        float angularVelocity_ = 0.0f;
        Vector2 gravity_ = {0.0f, 981.0f};
        GroundInfo groundInfo_;
        Vector2 velocity_ = {0.0f, 0.0f};

        // Config
        float linearDamping_ = 0.05f;
        float mass_ = 1.0f;
        float maxSpeed_ = 0.0f;
        float restitution_ = 1.0f;
        float friction_ = 0.0f;
        BodyType bodyType_ = BodyType::Dynamic;
    };
}
