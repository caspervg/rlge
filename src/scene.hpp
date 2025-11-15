#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "entity_registry.hpp"

namespace rlge {
    class Engine;
    class Entity;

    class Scene {
    public:
        explicit Scene(Engine& e);

        virtual ~Scene();

        virtual void enter();
        virtual void exit();
        virtual void pause();
        virtual void resume();

        virtual void update(float dt);
        virtual void draw();

        template <typename T, typename... Args>
        T& spawn(Args&&... args) {
            static_assert(std::is_base_of_v<Entity, T>, "T must be Entity");
            auto ent = std::make_unique<T>(*this, std::forward<Args>(args)...);
            T& ref = *ent;
            EntityId id = registry_.create(ent.get());
            ref.id_ = id;
            entities_.push_back(std::move(ent));
            return ref;
        }

        Entity* get(EntityId id) const;
        const std::vector<std::unique_ptr<Entity>>& entities();

        Engine& engine();
        const Engine& engine() const;

    private:
        Engine& engine_;
        EntityRegistry registry_;
        std::vector<std::unique_ptr<Entity>> entities_;
    };

    class SceneStack {
    public:
        void push(std::unique_ptr<Scene> s);
        void pop();
        void update(float dt);
        void draw();
        void drawDebug();

    private:
        std::vector<std::unique_ptr<Scene>> stack_;
    };
}
