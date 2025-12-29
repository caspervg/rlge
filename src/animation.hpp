#pragma once
#include <array>
#include <functional>
#include <stdexcept>
#include <string>

#include "component.hpp"
#include "sprite.hpp"

namespace rlge {

    class Entity;
    class SpriteAnim;

    struct AnimationClip {
        int startFrame{0};
        int frameCount{0};
        float frameTime{0.0f};
        bool loop{false};
        std::function<void()> onComplete;
    };

    template <typename StateEnum>
    class AnimationStateMachine : public Component {
    public:
        explicit AnimationStateMachine(Entity& entity, SpriteAnim& sprite)
        : Component(entity)
        , sprite_(sprite) {};

        void update(float dt) override {
            State& state = states_[currentStateIndex_];
            const AnimationClip& clip = state.clip;

            state.timer += dt;
            auto framesToAdvance = static_cast<int>(state.timer / clip.frameTime);

            if (framesToAdvance > 0) {
                state.currentFrame += framesToAdvance;
                state.timer -= clip.frameTime * framesToAdvance;

                if (state.currentFrame >= clip.frameCount) {
                    if (clip.loop) {
                        state.currentFrame = 0;
                    } else {
                        state.currentFrame = clip.frameCount - 1;
                        state.finished = true;
                        isFinished_ = true;

                        if (clip.onComplete) {
                            clip.onComplete();
                        }
                    }
                }

                updateFrame_();
            }
        }

        void registerClip(StateEnum state, const AnimationClip& clip) {
            const auto idx = static_cast<int>(state);
            checkStateIndex_(idx);
            states_[idx] = {clip, 0.0f, 0, false};
        }
        
        void setState(StateEnum state) {
            const auto idx = static_cast<int>(state);
            checkStateIndex_(idx);
            
            if (currentStateIndex_ == idx) return;
            
            auto& newState = states_[idx];
            currentStateIndex_ = idx;
            newState.timer = 0.0f;
            newState.currentFrame = 0;
            newState.finished = false;
            isFinished_ = false;
            
            updateFrame_();
        }

        [[nodiscard]] StateEnum currentState() const { return static_cast<StateEnum>(currentStateIndex_); }
        [[nodiscard]] bool isFinished() const { return isFinished_; }
        [[nodiscard]] bool isStateFinished(StateEnum state) const {
            const auto idx = static_cast<int>(state);
            checkStateIndex_(idx);
            return states_[idx].finished;
        }
        
        
    private:
        void updateFrame_() {
            const State& state = states_[currentStateIndex_];
            const AnimationClip& clip = state.clip;
            sprite_.setFrame(clip.startFrame + state.currentFrame);
        }
        static void checkStateIndex_(const int idx) {
            if (idx < 0 || idx >= MAX_STATES) {
                throw std::out_of_range{"State enum index out of bounds, the maximum is " + std::to_string(MAX_STATES)};
            }
        }

    private:
        static constexpr auto MAX_STATES = 64zu;

        struct State {
            AnimationClip clip;
            float timer{0.0f};
            int currentFrame{0};
            bool finished{false};
        };

        SpriteAnim& sprite_;
        int currentStateIndex_{0};
        std::array<State, MAX_STATES> states_;
        bool isFinished_{false};
    };

} // namespace rlge
