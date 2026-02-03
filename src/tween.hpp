#pragma once
#include <functional>
#include <vector>


namespace rlge {
    class Tween {
    public:
        using Easing = std::function<float(float)>;
        using Apply = std::function<void(float)>;
        using Complete = std::function<void()>;

        Tween(float duration, Apply apply, Easing ease, Complete complete = {});

        bool update(float dt);

    private:
        float t_ = 0.0f;
        float dur_;
        Apply apply_;
        Easing ease_;
        Complete complete_;
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
