#pragma once
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "asset.hpp"
#include "audio.hpp"
#include "camera.hpp"
#include "events.hpp"
#include "input.hpp"
#include "prefab.hpp"
#include "render_layer.hpp"
#include "render_queue.hpp"
#include "scene.hpp"
#include "transition.hpp"
#include "window.hpp"


namespace rlge {
    class Scene;

    struct View {
        ViewId id;
        Camera* camera;
        Rectangle viewport;
    };

    class GameServices {
    public:
        EventBus& gameEvents() { return events_; }
        AudioManager& audio() { return audio_; }
        PrefabFactory& prefabs() { return prefabs_; }

    private:
        EventBus events_;
        AudioManager audio_;
        PrefabFactory prefabs_;

        friend class Runtime;
    };

    class Runtime {
    public:
        explicit Runtime(const WindowConfig& cfg);
        ~Runtime();

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        // Immediately go to a new scene
        template <typename T, typename... Args>
        void pushScene(Args&&... args) {
            static_assert(std::is_base_of_v<Scene, T>, "T must be Scene");
            auto ptr = std::make_unique<T>(*this, std::forward<Args>(args)...);
            scenes_.push(std::move(ptr));
        }

        // Transition to a new scene
        template <typename T, typename... Args>
        void transitionTo(std::unique_ptr<Transition> transition, Args&&... args) {
            pendingTransition_ = std::move(transition);
            pendingSceneFactory_ = [this, ... args = std::forward<Args>(args)]() {
                return std::make_unique<T>(*this, args...);
            };
            transitionState_ = TransitionState::Out;
            pendingTransition_->setPhase(TransitionPhase::Out);
        }

        void popScene();
        void clearScenes();

        void run();

        void quit();

        AssetStore& assetStore();
        const AssetStore& assetStore() const;

        Input<>& input();
        const Input<>& input() const;

        RenderQueue& renderer();
        const RenderQueue& renderer() const;

        LayerRegistry& layers();
        const LayerRegistry& layers() const;

        GameServices& services();
        const GameServices& services() const;

        Window& window();
        const Window& window() const;

        ViewId addView(Camera& camera, const Rectangle& viewport);
        void clearViews();
        bool removeView(ViewId id);

        View* primaryView();
        [[nodiscard]] const View* primaryView() const;

        View* view(ViewId id);
        [[nodiscard]] const View* view(ViewId id) const;

        [[nodiscard]] const std::vector<View>& views() const;

    private:
        void updateTransition_(float dt);
        void drawTransition_();

    private:
        enum class TransitionState { None, Out, In };
        bool running_ = false;
        bool debugEnabled_ = false;
        KeyboardKey debugKey_ = KEY_F1;
        Window window_;
        AssetStore assets_;
        Input<> input_;
        GameServices services_;
        LayerRegistry layers_;
        RenderQueue renderer_;
        SceneStack scenes_;
        std::vector<View> views_;
        ViewId nextViewId_{0};
        std::unique_ptr<Transition> pendingTransition_;
        std::function<std::unique_ptr<Scene>()> pendingSceneFactory_;
        TransitionState transitionState_ = TransitionState::None;
    };
}
