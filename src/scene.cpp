#include "scene.hpp"
#include "runtime.hpp"

#include "debug.hpp"
#include "entity.hpp"

namespace rlge {
    Scene::Scene(Runtime& r)
        : runtime_(r)
        , ctx_{r.assetStore(), r.input(), r.renderer(),
               r.services().events(), r.services().audio(),
               r.services().camera()} {}

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

    Runtime& Scene::runtime() {
        return runtime_;
    }

    const Runtime& Scene::runtime() const {
        return runtime_;
    }

    AssetStore& Scene::assets() {
        return ctx_.assets;
    }

    const AssetStore& Scene::assets() const {
        return ctx_.assets;
    }

    Input& Scene::input() {
        return ctx_.input;
    }

    const Input& Scene::input() const {
        return ctx_.input;
    }

    RenderQueue& Scene::rq() {
        return ctx_.renderer;
    }

    const RenderQueue& Scene::rq() const {
        return ctx_.renderer;
    }

    EventBus& Scene::events() {
        return ctx_.events;
    }

    const EventBus& Scene::events() const {
        return ctx_.events;
    }

    AudioManager& Scene::audio() {
        return ctx_.audio;
    }

    const AudioManager& Scene::audio() const {
        return ctx_.audio;
    }

    Camera& Scene::camera() {
        return ctx_.camera;
    }

    const Camera& Scene::camera() const {
        return ctx_.camera;
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
