#include "transformer.hpp"
#include <algorithm>

namespace rlge {

Transform::~Transform() {
    // Remove self from parent's children list
    clearParent();

    // Clear all children's parent pointers (they become root transforms)
    for (auto* child : children_) {
        child->parent_ = nullptr;
    }
    children_.clear();
}

void Transform::setParent(Transform* parent) {
    if (parent_ == parent) return;

    // Remove from old parent's children
    if (parent_) {
        auto& siblings = parent_->children_;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    }

    parent_ = parent;

    // Add to new parent's children
    if (parent_) {
        parent_->children_.push_back(this);
    }
}

void Transform::clearParent() {
    setParent(static_cast<Transform*>(nullptr));
}

void Transform::setParent(Entity* parentEntity) {
    setParent(parentEntity ? parentEntity->get<Transform>() : nullptr);
}

Vector2 Transform::worldPosition() const {
    if (!parent_) return position;

    // Get parent's world transform
    const Vector2 parentWorldPos = parent_->worldPosition();
    const float parentWorldRot = parent_->worldRotation();
    const Vector2 parentWorldScale = parent_->worldScale();

    // Transform local position by parent's world transform
    // 1. Scale local position
    Vector2 scaled = Vector2Multiply(position, parentWorldScale);
    // 2. Rotate by parent's rotation
    Vector2 rotated = Vector2Rotate(scaled, parentWorldRot);
    // 3. Translate by parent's position
    return Vector2Add(parentWorldPos, rotated);
}

float Transform::worldRotation() const {
    if (!parent_) return rotation;
    return parent_->worldRotation() + rotation;
}

Vector2 Transform::worldScale() const {
    if (!parent_) return scale;
    return Vector2Multiply(parent_->worldScale(), scale);
}

Matrix Transform::worldMatrix() const {
    const Matrix local = matrix();
    if (!parent_) return local;
    return MatrixMultiply(local, parent_->worldMatrix());
}

void Transform::setWorldPosition(Vector2 worldPos) {
    if (!parent_) {
        position = worldPos;
        return;
    }

    // Convert world position to local space
    const Vector2 parentWorldPos = parent_->worldPosition();
    const float parentWorldRot = parent_->worldRotation();
    const Vector2 parentWorldScale = parent_->worldScale();

    // Inverse transform: world -> local
    // 1. Subtract parent position (inverse translate)
    Vector2 relativePos = Vector2Subtract(worldPos, parentWorldPos);
    // 2. Rotate by negative parent rotation (inverse rotate)
    relativePos = Vector2Rotate(relativePos, -parentWorldRot);
    // 3. Divide by parent scale (inverse scale)
    position = Vector2{
        relativePos.x / parentWorldScale.x,
        relativePos.y / parentWorldScale.y
    };
}

void Transform::setWorldRotation(float worldRot) {
    if (!parent_) {
        rotation = worldRot;
        return;
    }
    rotation = worldRot - parent_->worldRotation();
}

} // namespace rlge
