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
#include "tween.hpp"
#include "collision/collision_system.hpp"

namespace rlge {
    struct View;
    class Runtime;
    class Entity;

    using ViewId = std::uint32_t;

    struct GameContext {
        AssetStore& assets;
        Input& input;
        RenderQueue& renderer;
        EventBus& gameEvents;
        AudioManager& audio;
    };

    class ViewHandle {
    public:
        explicit ViewHandle(Runtime& r, const ViewId& view);
        ~ViewHandle();

    private:
        Runtime& runtime_;
        ViewId id_;
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

        AudioManager& audio();
        const AudioManager& audio() const;

        CollisionSystem& collisions();
        const CollisionSystem& collisions() const;

        CollisionResponseSystem& collisionResponses();
        const CollisionResponseSystem& collisionResponses() const;

        TweenSystem& tweens();
        const TweenSystem& tweens() const;

        EventBus& sceneEvents();
        const EventBus& sceneEvents() const;

        EventBus& gameEvents();
        const EventBus& gameEvents() const;

        void addView(Camera& camera, const Rectangle& viewport);
        [[nodiscard]] const View* primaryView() const;
        [[nodiscard]] const std::vector<View>& views() const;

        void setSingleView(Camera& cam);

        template <typename Event>
        void forwardGameEvent() {
            auto id = gameEvents().subscribe<Event>([this](const Event& e) {
                sceneEvents_.publish(e); // Forward to scene bus
            });
            forwardedGameSubscriptions_.push_back([this, id] {
                gameEvents().unsubscribe<Event>(id);
            });
        }

    private:
        Runtime& runtime_;
        GameContext ctx_;
        CollisionSystem collisions_;
        CollisionResponseSystem collisionResponses_;
        TweenSystem tweens_;
        EventBus sceneEvents_;
        EntityRegistry registry_;
        std::vector<std::unique_ptr<Entity>> entities_;
        std::vector<std::unique_ptr<ViewHandle>> viewHandles_;
        std::vector<std::function<void()>> forwardedGameSubscriptions_;
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
