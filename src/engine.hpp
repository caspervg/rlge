#pragma once
#include <memory>
#include <type_traits>
#include <utility>

#include "asset.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "collision.hpp"
#include "events.hpp"
#include "input.hpp"
#include "prefab.h"
#include "render_queue.hpp"
#include "scene.hpp"
#include "tween.hpp"


namespace rlge {
    class Scene;

    class GameServices {
    public:
        CollisionSystem& collisions() { return collisions_; }
        EventBus& events() { return events_; }
        TweenSystem& tweens() { return tweens_; }
        AudioManager& audio() { return audio_; }
        PrefabFactory& prefabs() { return prefabs_; }
        Camera& camera() { return camera_; }

    private:
        CollisionSystem collisions_;
        EventBus events_;
        TweenSystem tweens_;
        AudioManager audio_;
        PrefabFactory prefabs_;
        Camera camera_;

        friend class Engine;
    };

    class Engine {
    public:
        Engine(int width, int height, int fps, const char* title);
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        template <typename T, typename... Args>
        void pushScene(Args&&... args) {
            static_assert(std::is_base_of_v<Scene, T>, "T must be Scene");
            auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
            scenes_.push(std::move(ptr));
        }

        void popScene();
        void run();

        void quit();

        AssetStore& assetStore();
        const AssetStore& assetStore() const;

        Input& input();
        const Input& input() const;

        RenderQueue& renderer();
        const RenderQueue& renderer() const;

        GameServices& services();
        const GameServices& services() const;

    private:
        bool running_;
        bool debugEnabled_ = false;
        KeyboardKey debugKey_ = KEY_F1;
        AssetStore assets_;
        Input input_;
        GameServices services_;
        RenderQueue renderer_;
        SceneStack scenes_;
    };
}
