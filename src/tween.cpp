#include "tween.hpp"

namespace rlge {
    Tween::Tween(const float duration, const Apply& apply, const Easing& ease)
        : t_(0.0f)
        , dur_(duration)
        , apply_(apply)
        , ease_(ease) {}

    bool Tween::update(float dt) {
        t_ += dt;
        const float k = (t_ >= dur_) ? 1.0f : (t_ / dur_);
        apply_(ease_(k));
        return t_ >= dur_;
    }

    void TweenSystem::add(Tween tw) {
        tweens_.push_back(std::move(tw));
    }

    void TweenSystem::update(float dt) {
        for (size_t i = 0; i < tweens_.size();) {
            if (tweens_[i].update(dt)) {
                tweens_[i] = std::move(tweens_.back());
                tweens_.pop_back();
            } else {
                ++i;
            }
        }
    }

    float easeLinear(const float t) { return t; }
    float easeOutQuad(const float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
}

