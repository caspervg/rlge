#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "asset.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "entity_registry.hpp"
#include "events.hpp"
#include "input.hpp"
#include "render_layer.hpp"
#include "timer.hpp"
#include "tween.hpp"
#include "view.hpp"
#include "collision/collision_system.hpp"

namespace rlge {
    class Runtime;
    class Entity;

    struct GameContext {
        AssetStore& assets;
        Input<>& input;
        RenderQueue& renderer;
        LayerRegistry& layers;
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

    class Runtime;
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

        // Optional hooks to let scenes redirect world rendering (e.g., to render targets)
        // and run post-processing before UI is drawn.
        virtual RenderTexture2D* beginWorldRenderTarget() { return nullptr; }
        virtual void afterWorldRender(RenderTexture2D* target, const std::vector<View>& views) {}

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

        [[nodiscard]] Entity* get(EntityId id) const;
        const std::vector<std::unique_ptr<Entity>>& entities();

        void destroy(EntityId id);
        void destroyDeferred(EntityId id);

        Runtime& runtime();
        [[nodiscard]] const Runtime& runtime() const;

        AssetStore& assets();
        [[nodiscard]] const AssetStore& assets() const;

        Input<>& input();
        [[nodiscard]] const Input<>& input() const;

        RenderQueue& rq();
        [[nodiscard]] const RenderQueue& rq() const;

        LayerRegistry& layers();
        [[nodiscard]] const LayerRegistry& layers() const;

        AudioManager& audio();
        [[nodiscard]] const AudioManager& audio() const;

        CollisionSystem& collisions();
        [[nodiscard]] const CollisionSystem& collisions() const;

        CollisionResponseSystem& collisionResponses();
        [[nodiscard]] const CollisionResponseSystem& collisionResponses() const;

        TweenSystem& tweens();
        [[nodiscard]] const TweenSystem& tweens() const;

        TimerSystem& timers();
        [[nodiscard]] const TimerSystem& timers() const;

        EventBus& sceneEvents();
        [[nodiscard]] const EventBus& sceneEvents() const;

        EventBus& gameEvents();
        [[nodiscard]] const EventBus& gameEvents() const;

        void addView(Camera2DController& camera, const Rectangle& viewport,
            std::function<Rectangle(float width, float height)> onResize = nullptr,
            std::optional<ResizeMode> mode = std::nullopt,
            std::optional<float> aspectRatio = std::nullopt);
        void addView3D(Camera3DController& camera, const Rectangle& viewport,
            std::function<Rectangle(float width, float height)> onResize = nullptr,
            std::optional<ResizeMode> mode = std::nullopt,
            std::optional<float> aspectRatio = std::nullopt,
            Camera2DController* overlay2D = nullptr);
        [[nodiscard]] const View* primaryView() const;
        [[nodiscard]] const std::vector<View>& views() const;

        void setSingleView(Camera2DController& cam);

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
        TimerSystem timers_;
        EventBus sceneEvents_;
        EntityRegistry registry_;
        std::vector<std::unique_ptr<Entity>> entities_;
        std::vector<std::unique_ptr<ViewHandle>> viewHandles_;
        std::vector<std::function<void()>> forwardedGameSubscriptions_;
        std::vector<EntityId> pendingEntityDestructions_{};
    };

    class SceneStack {
    public:
        void push(std::unique_ptr<Scene> s);
        void pop();
        void clear();
        void update(float dt) const;
        void draw() const;
        void drawDebug() const;
        Scene* top();
        [[nodiscard]] const Scene* top() const;

        template<typename Fn>
        void forEach(Fn&& fn) {
            for (auto& s : stack_) {
                if (s) fn(*s);
            }
        }

        template<typename Fn>
        void forEach(Fn&& fn) const {
            for (const auto& s : stack_) {
                if (s) fn(*s);
            }
        }

    private:
        std::vector<std::unique_ptr<Scene>> stack_;
    };
}
