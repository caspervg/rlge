#include "scene.hpp"
#include "runtime.hpp"

#include "debug.hpp"
#include "entity.hpp"

namespace rlge {
    ViewHandle::ViewHandle(Runtime& r, const ViewId& view) : runtime_(r), id_(view) {}
    ViewHandle::~ViewHandle() { runtime_.removeView(id_); }

    Scene::Scene(Runtime& r) :
        runtime_(r), ctx_{r.assetStore(), r.input(), r.renderer(), r.services().events(), r.services().audio()} {}

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

    Entity* Scene::get(const EntityId id) const { return registry_.get(id); }

    const std::vector<std::unique_ptr<Entity>>& Scene::entities() { return entities_; }

    Runtime& Scene::runtime() { return runtime_; }

    const Runtime& Scene::runtime() const { return runtime_; }

    AssetStore& Scene::assets() { return ctx_.assets; }

    const AssetStore& Scene::assets() const { return ctx_.assets; }

    Input& Scene::input() { return ctx_.input; }

    const Input& Scene::input() const { return ctx_.input; }

    RenderQueue& Scene::rq() { return ctx_.renderer; }

    const RenderQueue& Scene::rq() const { return ctx_.renderer; }

    EventBus& Scene::events() { return ctx_.events; }

    const EventBus& Scene::events() const { return ctx_.events; }

    AudioManager& Scene::audio() { return ctx_.audio; }

    const AudioManager& Scene::audio() const { return ctx_.audio; }

    void Scene::addView(Camera& camera, const Rectangle& viewport) {
        const auto viewId = runtime_.addView(camera, viewport);
        viewHandles_.push_back(std::make_unique<ViewHandle>(runtime_, viewId));
    }

    const View* Scene::primaryView() const { return runtime_.primaryView(); }

    const std::vector<View>& Scene::views() const { return runtime_.views(); }

    void Scene::setSingleView(Camera& cam) {
        viewHandles_.clear();
        const auto [x, y] = runtime().window().size();
        addView(cam, Rectangle{0, 0, x, y});
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
} // namespace rlge
