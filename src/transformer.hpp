#pragma once
#include <vector>
#include "component.hpp"
#include "entity.hpp"
#include "raylib.h"
#include "raymath.h"

namespace rlge {
    class Transform : public Component {
    public:
        explicit Transform(Entity& e)
            : Component(e), position{0,0}, rotation(0.0f), scale{1.0f,1.0f}, parent_(nullptr) {}

        // Constructor with parent - useful for creating child entities
        Transform(Entity& e, Transform* parent)
            : Component(e), position{0,0}, rotation(0.0f), scale{1.0f,1.0f}, parent_(nullptr) {
            setParent(parent);
        }

        // Syntactic sugar: construct with parent entity
        Transform(Entity& e, Entity& parentEntity)
            : Component(e), position{0,0}, rotation(0.0f), scale{1.0f,1.0f}, parent_(nullptr) {
            // Try to get parent's transform
            if (auto* parentTransform = parentEntity.get<Transform>()) {
                setParent(parentTransform);
            }
            // else: parent_ remains nullptr
        }

        // Destructor - clean up hierarchy relationships
        ~Transform() override;

        // Local transform properties (relative to parent, or world if no parent)
        Vector2 position;
        float   rotation;
        Vector2 scale;

        // Parent-child hierarchy
        void setParent(Transform* parent);
        void setParent(Entity* parentEntity);
        void clearParent();
        void detachFromParent();
        [[nodiscard]] bool hasParent() const { return parent_ != nullptr; }
        [[nodiscard]] Transform* parent() const { return parent_; }
        [[nodiscard]] const std::vector<Transform*>& children() const { return children_; }

        // World space accessors
        [[nodiscard]] Vector2 worldPosition() const;
        [[nodiscard]] float worldRotation() const;
        [[nodiscard]] Vector2 worldScale() const;
        [[nodiscard]] Matrix worldMatrix() const;

        // World space setters (converts to local space if has parent)
        void setWorldPosition(Vector2 worldPos);
        void setWorldRotation(float worldRot);

        // Local matrix (T * R * S)
        [[nodiscard]] Matrix matrix() const {
            const auto t = MatrixTranslate(position.x, position.y, 0.0f);
            const auto r = MatrixRotateZ(rotation);
            const auto s = MatrixScale(scale.x, scale.y, 1.0f);

            // Local = T * R * S
            return MatrixMultiply(s, MatrixMultiply(r, t));
        }

        [[nodiscard]] Vector2 right() const {
            return Vector2Rotate({1.0f, 0.0f}, rotation);
        }

        [[nodiscard]] Vector2 up() const {
            return Vector2Rotate({0.0f, 1.0f}, rotation);
        }

    private:
        Transform* parent_;
        std::vector<Transform*> children_;
    };
}
