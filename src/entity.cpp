#include "entity.hpp"

#include "component.hpp"
#include "scene.hpp"

namespace rlge {
    Entity::~Entity() = default;

    EntityId Entity::id() const {
        return id_;
    }

    void Entity::update(const float dt) {
        for (auto& c : components_)
            c->update(dt);
    }

    void Entity::draw() {
        for (auto& c : components_)
            c->draw();
    }

    void Entity::destroy() const {
        scene_.destroy(id_);
    }

    void Entity::destroyDeferred() const {
        scene_.destroyDeferred(id_);
    }

    Scene& Entity::scene() {
        return scene_;
    }

    const Scene& Entity::scene() const {
        return scene_;
    }
}

