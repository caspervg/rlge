#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "entity.hpp"
#include "entity_registry.hpp"
#include "debug.hpp"

namespace rlge {
    class Scene {
    public:
        explicit Scene(Engine& e) :
            engine_(e) {}

        virtual ~Scene() = default;

        virtual void enter() {}
        virtual void exit() {}
        virtual void pause() {}
        virtual void resume() {}

        virtual void update(float dt) {
            for (auto& e : entities_)
                e->update(dt);
        }

        virtual void draw() {
            for (auto& e : entities_)
                e->draw();
        }

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

        Entity* get(const EntityId id) const { return registry_.get(id); }
        const std::vector<std::unique_ptr<Entity>>& entities() { return entities_; }

        Engine& engine() { return engine_; }
        const Engine& engine() const { return engine_; }

    private:
        Engine& engine_;
        EntityRegistry registry_;
        std::vector<std::unique_ptr<Entity>> entities_;
    };

    class SceneStack {
    public:
        void push(std::unique_ptr<Scene> s) {
            if (!stack_.empty())
                stack_.back()->pause();
            stack_.push_back(std::move(s));
            stack_.back()->enter();
        }

        void pop() {
            if (stack_.empty())
                return;
            stack_.back()->exit();
            stack_.pop_back();
            if (!stack_.empty())
                stack_.back()->resume();
        }

        void update(float dt) {
            if (stack_.empty())
                return;
            stack_.back()->update(dt);
        }

        void draw() {
            for (auto& s : stack_)
                s->draw();
        }

        // Invoke debug overlays on scenes that opt into HasDebugOverlay.
        void drawDebug() {
            for (auto& s : stack_) {
                if (auto* dbg = dynamic_cast<HasDebugOverlay*>(s.get())) {
                    dbg->debugOverlay();
                }
            }
        }

    private:
        std::vector<std::unique_ptr<Scene>> stack_;
    };
}
