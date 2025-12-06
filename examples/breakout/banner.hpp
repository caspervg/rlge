#pragma once
#include <string>

#include "render_entity.hpp"
#include "timer.hpp"

namespace breakout {
    // Simple HUD banner that hides itself after a countdown
    class Banner : public rlge::RenderEntity {
    public:
        explicit Banner(rlge::Scene& s);

        // Show a banner for the given duration; replaces any active banner
        void show(std::string text, float durationSeconds);
        void hide();

        void update(float dt) override;
        void draw() override;

    private:
        std::string text_;
        bool visible_{false};
        float remaining_{0.0f};
        rlge::TimerComponent* timers_{nullptr};
        rlge::CountdownHandle countdown_{};
    };
} // namespace breakout

