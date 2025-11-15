#include "scene.hpp"

#include "debug.hpp"
#include "entity.hpp"

namespace rlge {
    Scene::Scene(Engine& e)
        : engine_(e) {}

    Scene::~Scene() = default;

    void Scene::enter() {}
    void Scene::exit() {}
    void Scene::pause() {}
    void Scene::resume() {}

    void Scene::update(float dt) {
        for (auto& e : entities_)
            e->update(dt);
    }

    void Scene::draw() {
        for (auto& e : entities_)
            e->draw();
    }

    Entity* Scene::get(const EntityId id) const {
        return registry_.get(id);
    }

    const std::vector<std::unique_ptr<Entity>>& Scene::entities() {
        return entities_;
    }

    Engine& Scene::engine() {
        return engine_;
    }

    const Engine& Scene::engine() const {
        return engine_;
    }

    void SceneStack::push(std::unique_ptr<Scene> s) {
        if (!stack_.empty())
            stack_.back()->pause();
        stack_.push_back(std::move(s));
        stack_.back()->enter();
    }

    void SceneStack::pop() {
        if (stack_.empty())
            return;
        stack_.back()->exit();
        stack_.pop_back();
        if (!stack_.empty())
            stack_.back()->resume();
    }

    void SceneStack::update(float dt) {
        if (stack_.empty())
            return;
        stack_.back()->update(dt);
    }

    void SceneStack::draw() {
        for (auto& s : stack_)
            s->draw();
    }

    void SceneStack::drawDebug() {
        for (auto& s : stack_) {
            if (auto* dbg = dynamic_cast<HasDebugOverlay*>(s.get())) {
                dbg->debugOverlay();
            }
        }
    }
}

