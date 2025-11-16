#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "asset.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "entity_registry.hpp"
#include "events.hpp"
#include "input.hpp"
#include "render_queue.hpp"

namespace rlge {
    class Runtime;
    class Entity;

    struct GameContext {
        AssetStore& assets;
        Input& input;
        RenderQueue& renderer;
        EventBus& events;
        AudioManager& audio;
        Camera& camera;
    };

    class Scene {
    public:
        explicit Scene(Runtime& r);

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

        Runtime& runtime();
        const Runtime& runtime() const;

        AssetStore& assets();
        const AssetStore& assets() const;

        Input& input();
        const Input& input() const;

        RenderQueue& rq();
        const RenderQueue& rq() const;

        EventBus& events();
        const EventBus& events() const;

        AudioManager& audio();
        const AudioManager& audio() const;

        Camera& camera();
        const Camera& camera() const;

    private:
        Runtime& runtime_;
        GameContext ctx_;
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
