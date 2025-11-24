#include "physics_body.hpp"

#include <print>
#include <algorithm>

#include "transformer.hpp"

namespace rlge {

    const GroundInfo& PhysicsBody::groundInfo() const {
        return groundInfo_;
    }

    bool PhysicsBody::isGrounded() const {
        return groundInfo_.grounded;
    }

    void PhysicsBody::update(const float dt) {
        auto* tr = entity().get<Transform>();
        if (!tr)
            return;

        // ReSharper disable once CppDFAConstantConditions
        if (bodyType_ == BodyType::Kinematic) {
            groundInfo_.grounded = false;
            return;
        }

        // Apply gravity
        acceleration_ += gravity_;

        // Integrate velocity
        velocity_ += acceleration_ * dt;

        // Apply damping
        // velocity_ = Vector2Scale(velocity_, 1.0f - linearDamping_);

        // Clamp speed if that is set
        if (maxSpeed_ != 0.0f) {
            const float speed = Vector2Length(velocity_);
            if (speed > maxSpeed_) {
                velocity_ = Vector2Scale(velocity_, maxSpeed_ / speed);
            }
        }

        tr->position = Vector2Add(tr->position, Vector2Scale(velocity_, dt));

        if (angularVelocity_ != 0.0f) {
            tr->rotation += angularVelocity_ * dt;
        }

        // Clear grounded flag, which will be set by the collision handler if still grounded
        groundInfo_.grounded = false;

        // Clear acceleration each frame (forces are one time)
        acceleration_ = {0.0f, 0.0f};
    }

    void PhysicsBody::onCollision(const CollisionEvent& event) {
        if (event.state == CollisionState::Exit) {
            groundInfo_.grounded = false;
            return;
        }

        if (bodyType_ != BodyType::Dynamic) {
            return;
        }

        // Check if we're grounded (i.e. normal points roughly upward)
        // Threshold of -0.3 allows for +/- 70 degree slopes
        if (event.manifold.normal.y < -0.3f) {
            groundInfo_.grounded = true;
            groundInfo_.normal = event.manifold.normal;
            groundInfo_.collider = event.colliderB;
        }

        if (event.state == CollisionState::Enter) {
            reflectOffNormal(event.manifold.normal);
        }
    }

    void PhysicsBody::reflectOffNormal(const Vector2& normal) {
        // Split velocity into normal and tangential components.
        const float vn = Vector2DotProduct(velocity_, normal);
        const Vector2 vNorm = Vector2Scale(normal, vn);
        const Vector2 vTang = Vector2Subtract(velocity_, vNorm);

        // Apply restitution on the normal component (bounce) and friction on a tangential component (slide loss).
        const float e = std::clamp(restitution_, 0.0f, 1.5f); // allow slight >1 for lively bounces if desired
        const float f = std::clamp(friction_, 0.0f, 1.0f);

        const Vector2 newV = Vector2Add(
            Vector2Scale(vTang, 1.0f - f),
            Vector2Scale(vNorm, -e)
        );
        velocity_ = newV;
    }

    void PhysicsBody::applyForce(const Vector2& force) {
        acceleration_ = Vector2Add(acceleration_, Vector2Scale(force, 1.0f / mass_));
    }

}
