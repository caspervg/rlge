#pragma once
#include <functional>
#include <utility>
#include <vector>

namespace rlge {
    class Tween {
    public:
        using Easing = std::function<float(float)>;
        using Apply = std::function<void(float)>;

        Tween(float duration, const Apply& apply, const Easing& ease);

        bool update(float dt);

    private:
        float t_ = 0.0f;
        float dur_;
        Apply apply_;
        Easing ease_;
    };

    class TweenSystem {
    public:
        void add(Tween tw);

        void update(float dt);

    private:
        std::vector<Tween> tweens_;
    };

    float easeLinear(float t);
    float easeOutQuad(float t);

}
