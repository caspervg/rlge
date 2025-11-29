#include "transition.hpp"

#include <algorithm>

#include "easing.hpp"

namespace rlge {

    Transition::Transition(const float duration) :
        duration_(duration) {}

    bool Transition::update(const float dt) {
        elapsed_ += dt;
        progress_ = std::min(elapsed_ / duration_, 1.0f);
        return progress_ >= 1.0f;
    }

    void Transition::draw(RenderQueue&, float, float) {
        // Default transition draws nothing; derived classes override.
    }

    void Transition::setPhase(const TransitionPhase phase) { phase_ = phase; }

    FadeTransition::FadeTransition(const float duration, const Color color) :
        Transition(duration), color_(color) {}

    void FadeTransition::draw(RenderQueue& rq, float screenWidth, float screenHeight) {
        const auto alpha = (phase() == TransitionPhase::Out) ? progress() : 1.0f - progress();
        auto c = color_;
        c.a = static_cast<unsigned char>(alpha * 255.0f);
        rq.submitUI([c, screenWidth, screenHeight]() {
            DrawRectangle(0, 0, static_cast<int>(screenWidth), static_cast<int>(screenHeight), c);
        });
    }

    SlideTransition::SlideTransition(const float duration, const Direction direction) :
        Transition(duration), direction_(direction) {}

    void SlideTransition::draw(RenderQueue& rq, float screenWidth, float screenHeight) {
        const auto t = (phase() == TransitionPhase::Out) ? progress() : 1.0f - progress();
        const auto offset = easeInOutQuad(t);

        auto x = 0, y = 0;
        switch (direction_) {
        case Direction::Left:
            x = static_cast<int>(-screenWidth * (1.0f - offset));
            break;
        case Direction::Right:
            x = static_cast<int>(screenWidth * (1.0f - offset));
            break;
        case Direction::Up:
            y = static_cast<int>(-screenHeight * (1.0f - offset));
            break;
        case Direction::Down:
            y = static_cast<int>(screenHeight * (1.0f - offset));
            break;
        }

        rq.submitUI([x, y, screenWidth, screenHeight]() {
            DrawRectangle(x, y, static_cast<int>(screenWidth), static_cast<int>(screenHeight), BLACK);
        });
    }


}
