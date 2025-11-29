#pragma once
#include "raylib.h"
#include "render_queue.hpp"

namespace rlge {
    enum class TransitionPhase {
        Out,    // Covering the old scene
        In      // Revealing the new scene
    };

    class Transition {
    public:
        explicit Transition(float duration);
        virtual ~Transition() = default;

        // Returns true when a phase is completed
        bool update(float dt);

        virtual void draw(RenderQueue& rq, float screenWidth, float screenHeight);

        void setPhase(TransitionPhase phase);

        [[nodiscard]] float duration() const { return duration_; }
        [[nodiscard]] float elapsed() const { return elapsed_; }
        [[nodiscard]] float progress() const { return progress_; }
        [[nodiscard]] TransitionPhase phase() const { return phase_; }

    private:
        float duration_{0.0f};
        float elapsed_{0.0f};
        float progress_{0.0f};  // 0 to 1
        TransitionPhase phase_{TransitionPhase::Out};
    };

    class FadeTransition : public Transition {
    public:
        explicit FadeTransition(float duration = 0.3f, Color color = BLACK);
        void draw(RenderQueue& rq, float screenWidth, float screenHeight) override;
    private:
        Color color_{BLACK};
    };

    class SlideTransition : public Transition {
    public:
        enum class Direction { Left, Right, Up, Down };
        explicit SlideTransition(float duration = 0.4f, Direction direction = Direction::Left);
        void draw(RenderQueue& rq, float screenWidth, float screenHeight) override;
    private:
        Direction direction_{Direction::Left};
    };
}
