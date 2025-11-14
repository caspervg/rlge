#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace rlge {
    class Tween {
    public:
        using Easing = std::function<float(float)>;
        using Apply = std::function<void(float)>;

        Tween(const float duration, const Apply& apply, const Easing& ease) :
            dur_(duration), apply_(apply), ease_(ease) {}

        bool update(float dt) {
            t_ += dt;
            const float k = (t_ >= dur_) ? 1.0f : (t_ / dur_);
            apply_(ease_(k));
            return t_ >= dur_;
        }

    private:
        float t_ = 0.0f;
        float dur_;
        Apply apply_;
        Easing ease_;
    };

    class TweenSystem {
    public:
        void add(Tween tw) {
            tweens_.push_back(std::move(tw));
        }

        void update(float dt) {
            for (size_t i = 0; i < tweens_.size();) {
                if (tweens_[i].update(dt)) {
                    tweens_[i] = std::move(tweens_.back());
                    tweens_.pop_back();
                }
                else {
                    ++i;
                }
            }
        }

    private:
        std::vector<Tween> tweens_;
    };

    inline float easeLinear(const float t) { return t; }
    inline float easeOutQuad(const float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

}
