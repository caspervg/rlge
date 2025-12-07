#pragma once
#include <box2d/box2d.h>
#include <memory>
#include <unordered_map>
#include "component.hpp"
#include "raylib.h"
#include "collision/collider_types.hpp"
#include "debug.hpp"

namespace rlge {
    class Entity;
    class Box2DPhysicsWorld;

    // Configuration for a Box2D body
    struct Box2DBodyConfig {
        b2BodyType bodyType = b2_dynamicBody;
        Vector2 initialVelocity{0.0f, 0.0f};
        float gravityScale = 1.0f;
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;
        bool fixedRotation = false;
    };

    // Configuration for a Box2D fixture
    struct Box2DFixtureConfig {
        float density = 1.0f;
        float friction = 0.3f;
        float restitution = 0.0f;
        bool isSensor = false;
        ColliderLayerMask layer = ColliderLayerMask::LAYER_WORLD;
        ColliderLayerMask mask = ColliderLayerMask::LAYER_WORLD;
    };

    // Box2D body component that syncs with entity Transform
    class Box2DBody : public Component {
    public:
        Box2DBody(Entity& e, Box2DPhysicsWorld& world, const Box2DBodyConfig& cfg);
        ~Box2DBody() override;

        void update(float dt) override;

        // Body accessors
        b2Body* body() { return body_; }
        [[nodiscard]] const b2Body* body() const { return body_; }

        // Add shape fixtures
        b2Fixture* addBoxFixture(float width, float height, const Box2DFixtureConfig& cfg = {});
        b2Fixture* addCircleFixture(float radius, const Vector2& center = {0.0f, 0.0f}, const Box2DFixtureConfig& cfg = {});
        b2Fixture* addPolygonFixture(const std::vector<Vector2>& points, const Box2DFixtureConfig& cfg = {});

        // Velocity control
        void setVelocity(const Vector2& vel);
        [[nodiscard]] Vector2 getVelocity() const;

        // Enable/disable the body
        void setEnabled(bool enabled);
        [[nodiscard]] bool isEnabled() const;

        // Apply forces/impulses
        void applyForce(const Vector2& force, const Vector2& point);
        void applyForceToCenter(const Vector2& force);
        void applyLinearImpulse(const Vector2& impulse, const Vector2& point);
        void applyLinearImpulseToCenter(const Vector2& impulse);

        // Callbacks for collision events
        void setOnCollisionEnter(CollisionCallback cb) { onCollisionEnter_ = std::move(cb); }
        void setOnCollisionExit(CollisionCallback cb) { onCollisionExit_ = std::move(cb); }
        void setOnCollisionStay(CollisionCallback cb) { onCollisionStay_ = std::move(cb); }

        void onCollision(const CollisionEvent& e);

    private:
        Box2DPhysicsWorld& world_;
        b2Body* body_;
        CollisionCallback onCollisionEnter_;
        CollisionCallback onCollisionExit_;
        CollisionCallback onCollisionStay_;
    };

    // Contact listener that generates collision events
    class Box2DContactListener : public b2ContactListener {
    public:
        void BeginContact(b2Contact* contact) override;
        void EndContact(b2Contact* contact) override;
        void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override;

        [[nodiscard]] const std::vector<CollisionEvent>& getCollisionEvents() const { return events_; }
        void clearEvents() { events_.clear(); }

    private:
        std::vector<CollisionEvent> events_;
        std::unordered_map<b2Contact*, bool> activeContacts_;
    };

    // Box2D world manager per scene
    class Box2DPhysicsWorld : public HasDebugOverlay {
    public:
        explicit Box2DPhysicsWorld(const Vector2& gravity = {0.0f, 981.0f});
        ~Box2DPhysicsWorld();

        void step(float dt);
        void draw(); // Call to render debug shapes
        void setGravity(const Vector2& gravity);
        [[nodiscard]] Vector2 getGravity() const;

        b2World* world() { return world_.get(); }
        [[nodiscard]] const b2World* world() const { return world_.get(); }

        Box2DContactListener& contactListener() { return contactListener_; }
        [[nodiscard]] const Box2DContactListener& contactListener() const { return contactListener_; }

        void setDebug(bool debug) { debug_ = debug; }
        [[nodiscard]] bool debug() const { return debug_; }
        
        void debugOverlay() override;

        // Create a body (called by Box2DBody component)
        b2Body* createBody(const b2BodyDef& def);
        void destroyBody(b2Body* body);

    private:
        std::unique_ptr<b2World> world_;
        Box2DContactListener contactListener_;
        std::unique_ptr<class Box2DDebugDraw> debugDraw_;
        bool debug_ = false;
        float timeAccumulator_ = 0.0f;
        static constexpr float fixedTimeStep_ = 1.0f / 60.0f;
    };
}
