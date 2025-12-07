#include "box2d_physics.hpp"
#include "box2d_debug.hpp"
#include "entity.hpp"
#include "transformer.hpp"
#include "imgui.h"
#include <algorithm>

namespace rlge {
    namespace {
        // Convert raylib Vector2 to Box2D b2Vec2
        inline b2Vec2 toB2Vec2(const Vector2& v) {
            return b2Vec2(v.x, v.y);
        }

        // Convert Box2D b2Vec2 to raylib Vector2
        inline Vector2 toVector2(const b2Vec2& v) {
            return Vector2{v.x, v.y};
        }

        // Convert layer mask to Box2D category/mask bits
        inline uint16_t layerToCategory(ColliderLayerMask layer) {
            return static_cast<uint16_t>(layer);
        }

        inline uint16_t maskToBits(ColliderLayerMask mask) {
            return static_cast<uint16_t>(mask);
        }
    }

    // Box2DPhysicsWorld implementation
    Box2DPhysicsWorld::Box2DPhysicsWorld(const Vector2& gravity) {
        b2Vec2 b2Gravity = toB2Vec2(gravity);
        world_ = std::make_unique<b2World>(b2Gravity);
        world_->SetContactListener(&contactListener_);
        debugDraw_ = std::make_unique<Box2DDebugDraw>();
        world_->SetDebugDraw(debugDraw_.get());
    }

    Box2DPhysicsWorld::~Box2DPhysicsWorld() = default;

    void Box2DPhysicsWorld::step(float dt) {
        // Use fixed timestep for stability
        timeAccumulator_ += dt;
        while (timeAccumulator_ >= fixedTimeStep_) {
            contactListener_.clearEvents();
            world_->Step(fixedTimeStep_, 8, 3);
            timeAccumulator_ -= fixedTimeStep_;
        }
    }

    void Box2DPhysicsWorld::draw() {
        if (debugDraw_ && debugDraw_->enabled()) {
            world_->DebugDraw();
        }
    }

    void Box2DPhysicsWorld::debugOverlay() {
        if (debugDraw_) {
            debugDraw_->debugOverlay();
        }
    }

    void Box2DPhysicsWorld::setGravity(const Vector2& gravity) {
        world_->SetGravity(toB2Vec2(gravity));
    }

    Vector2 Box2DPhysicsWorld::getGravity() const {
        return toVector2(world_->GetGravity());
    }

    b2Body* Box2DPhysicsWorld::createBody(const b2BodyDef& def) {
        return world_->CreateBody(&def);
    }

    void Box2DPhysicsWorld::destroyBody(b2Body* body) {
        if (body) {
            world_->DestroyBody(body);
        }
    }

    // Box2DBody implementation
    Box2DBody::Box2DBody(Entity& e, Box2DPhysicsWorld& world, const Box2DBodyConfig& cfg)
        : Component(e), world_(world) {
        
        b2BodyDef bodyDef;
        bodyDef.type = cfg.bodyType;
        
        // Get initial position from transform
        if (auto* tr = entity().get<Transform>()) {
            bodyDef.position = toB2Vec2(tr->position);
            bodyDef.angle = tr->rotation * DEG2RAD;
        }

        bodyDef.linearVelocity = toB2Vec2(cfg.initialVelocity);
        bodyDef.gravityScale = cfg.gravityScale;
        bodyDef.linearDamping = cfg.linearDamping;
        bodyDef.angularDamping = cfg.angularDamping;
        bodyDef.fixedRotation = cfg.fixedRotation;
        bodyDef.userData.pointer = reinterpret_cast<uintptr_t>(this);

        body_ = world_.createBody(bodyDef);
    }

    Box2DBody::~Box2DBody() {
        if (body_) {
            world_.destroyBody(body_);
            body_ = nullptr;
        }
    }

    void Box2DBody::update(float dt) {
        Component::update(dt);
        
        // Sync Box2D body position/rotation to entity Transform
        if (auto* tr = entity().get<Transform>()) {
            const b2Vec2 pos = body_->GetPosition();
            tr->position = toVector2(pos);
            tr->rotation = body_->GetAngle() * RAD2DEG;
        }
    }

    b2Fixture* Box2DBody::addBoxFixture(float width, float height, const Box2DFixtureConfig& cfg) {
        b2PolygonShape box;
        box.SetAsBox(width / 2.0f, height / 2.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &box;
        fixtureDef.density = cfg.density;
        fixtureDef.friction = cfg.friction;
        fixtureDef.restitution = cfg.restitution;
        fixtureDef.isSensor = cfg.isSensor;
        fixtureDef.filter.categoryBits = layerToCategory(cfg.layer);
        fixtureDef.filter.maskBits = maskToBits(cfg.mask);

        return body_->CreateFixture(&fixtureDef);
    }

    b2Fixture* Box2DBody::addCircleFixture(float radius, const Vector2& center, const Box2DFixtureConfig& cfg) {
        b2CircleShape circle;
        circle.m_radius = radius;
        circle.m_p = toB2Vec2(center);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circle;
        fixtureDef.density = cfg.density;
        fixtureDef.friction = cfg.friction;
        fixtureDef.restitution = cfg.restitution;
        fixtureDef.isSensor = cfg.isSensor;
        fixtureDef.filter.categoryBits = layerToCategory(cfg.layer);
        fixtureDef.filter.maskBits = maskToBits(cfg.mask);

        return body_->CreateFixture(&fixtureDef);
    }

    b2Fixture* Box2DBody::addPolygonFixture(const std::vector<Vector2>& points, const Box2DFixtureConfig& cfg) {
        if (points.size() < 3 || points.size() > b2_maxPolygonVertices) {
            return nullptr;
        }

        b2Vec2 vertices[b2_maxPolygonVertices];
        for (size_t i = 0; i < points.size(); ++i) {
            vertices[i] = toB2Vec2(points[i]);
        }

        b2PolygonShape polygon;
        polygon.Set(vertices, static_cast<int32>(points.size()));

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &polygon;
        fixtureDef.density = cfg.density;
        fixtureDef.friction = cfg.friction;
        fixtureDef.restitution = cfg.restitution;
        fixtureDef.isSensor = cfg.isSensor;
        fixtureDef.filter.categoryBits = layerToCategory(cfg.layer);
        fixtureDef.filter.maskBits = maskToBits(cfg.mask);

        return body_->CreateFixture(&fixtureDef);
    }

    void Box2DBody::setVelocity(const Vector2& vel) {
        body_->SetLinearVelocity(toB2Vec2(vel));
    }

    Vector2 Box2DBody::getVelocity() const {
        return toVector2(body_->GetLinearVelocity());
    }

    void Box2DBody::setEnabled(bool enabled) {
        body_->SetEnabled(enabled);
    }

    bool Box2DBody::isEnabled() const {
        return body_->IsEnabled();
    }

    void Box2DBody::applyForce(const Vector2& force, const Vector2& point) {
        body_->ApplyForce(toB2Vec2(force), toB2Vec2(point), true);
    }

    void Box2DBody::applyForceToCenter(const Vector2& force) {
        body_->ApplyForceToCenter(toB2Vec2(force), true);
    }

    void Box2DBody::applyLinearImpulse(const Vector2& impulse, const Vector2& point) {
        body_->ApplyLinearImpulse(toB2Vec2(impulse), toB2Vec2(point), true);
    }

    void Box2DBody::applyLinearImpulseToCenter(const Vector2& impulse) {
        body_->ApplyLinearImpulseToCenter(toB2Vec2(impulse), true);
    }

    void Box2DBody::onCollision(const CollisionEvent& e) {
        if (e.state == CollisionState::Enter && onCollisionEnter_) {
            onCollisionEnter_(e);
        } else if (e.state == CollisionState::Stay && onCollisionStay_) {
            onCollisionStay_(e);
        } else if (e.state == CollisionState::Exit && onCollisionExit_) {
            onCollisionExit_(e);
        }
    }

    // Box2DContactListener implementation
    void Box2DContactListener::BeginContact(b2Contact* contact) {
        auto* bodyA = contact->GetFixtureA()->GetBody();
        auto* bodyB = contact->GetFixtureB()->GetBody();

        auto* box2dBodyA = reinterpret_cast<Box2DBody*>(bodyA->GetUserData().pointer);
        auto* box2dBodyB = reinterpret_cast<Box2DBody*>(bodyB->GetUserData().pointer);

        if (!box2dBodyA || !box2dBodyB) return;

        // Create collision event
        CollisionEvent event;
        event.colliderA = nullptr; // Box2D doesn't use old Collider type
        event.colliderB = nullptr;
        event.entityA = &box2dBodyA->entity();
        event.entityB = &box2dBodyB->entity();
        event.state = CollisionState::Enter;
        
        // Get contact manifold
        b2WorldManifold worldManifold;
        contact->GetWorldManifold(&worldManifold);
        
        event.manifold.colliding = true;
        event.manifold.normal = toVector2(worldManifold.normal);
        // Use first contact point if available, otherwise use zero
        event.manifold.contactPoint = contact->GetManifold()->pointCount > 0 
            ? toVector2(worldManifold.points[0]) 
            : Vector2{0.0f, 0.0f};
        
        events_.push_back(event);
        activeContacts_[contact] = true;

        // Notify both bodies
        box2dBodyA->onCollision(event);
        
        // Swap for body B
        std::swap(event.entityA, event.entityB);
        event.manifold.normal = {-event.manifold.normal.x, -event.manifold.normal.y};
        box2dBodyB->onCollision(event);
    }

    void Box2DContactListener::EndContact(b2Contact* contact) {
        auto* bodyA = contact->GetFixtureA()->GetBody();
        auto* bodyB = contact->GetFixtureB()->GetBody();

        auto* box2dBodyA = reinterpret_cast<Box2DBody*>(bodyA->GetUserData().pointer);
        auto* box2dBodyB = reinterpret_cast<Box2DBody*>(bodyB->GetUserData().pointer);

        if (!box2dBodyA || !box2dBodyB) return;

        CollisionEvent event;
        event.colliderA = nullptr;
        event.colliderB = nullptr;
        event.entityA = &box2dBodyA->entity();
        event.entityB = &box2dBodyB->entity();
        event.state = CollisionState::Exit;
        event.manifold.colliding = false;

        events_.push_back(event);
        activeContacts_.erase(contact);

        box2dBodyA->onCollision(event);
        
        std::swap(event.entityA, event.entityB);
        box2dBodyB->onCollision(event);
    }

    void Box2DContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold) {
        // Check if this is an ongoing contact
        if (activeContacts_.find(contact) != activeContacts_.end()) {
            auto* bodyA = contact->GetFixtureA()->GetBody();
            auto* bodyB = contact->GetFixtureB()->GetBody();

            auto* box2dBodyA = reinterpret_cast<Box2DBody*>(bodyA->GetUserData().pointer);
            auto* box2dBodyB = reinterpret_cast<Box2DBody*>(bodyB->GetUserData().pointer);

            if (!box2dBodyA || !box2dBodyB) return;

            CollisionEvent event;
            event.colliderA = nullptr;
            event.colliderB = nullptr;
            event.entityA = &box2dBodyA->entity();
            event.entityB = &box2dBodyB->entity();
            event.state = CollisionState::Stay;
            
            b2WorldManifold worldManifold;
            contact->GetWorldManifold(&worldManifold);
            
            event.manifold.colliding = true;
            event.manifold.normal = toVector2(worldManifold.normal);
            // Use first contact point if available, otherwise use zero
            event.manifold.contactPoint = contact->GetManifold()->pointCount > 0 
                ? toVector2(worldManifold.points[0]) 
                : Vector2{0.0f, 0.0f};

            events_.push_back(event);

            box2dBodyA->onCollision(event);
            
            std::swap(event.entityA, event.entityB);
            event.manifold.normal = {-event.manifold.normal.x, -event.manifold.normal.y};
            box2dBodyB->onCollision(event);
        }
    }
}
