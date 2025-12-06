#include "banner.hpp"

#include "breakout_config.hpp"
#include <algorithm>
#include "raylib.h"

namespace breakout {
    using namespace rlge;

    Banner::Banner(Scene& s) :
        RenderEntity(s) {
        timers_ = &add<TimerComponent>();
    }

    void Banner::show(std::string text, const float durationSeconds) {
        text_ = std::move(text);
        visible_ = true;
        remaining_ = durationSeconds;

        if (countdown_) {
            timers_->cancel(countdown_);
        }

        countdown_ = timers_->countdown(
            durationSeconds,
            nullptr,
            [this]() {
                visible_ = false;
                text_.clear();
                countdown_ = {};
            }
        );
    }

    void Banner::hide() {
        visible_ = false;
        text_.clear();
        remaining_ = 0.0f;
        if (countdown_) {
            timers_->cancel(countdown_);
            countdown_ = {};
        }
    }

    void Banner::update(const float dt) {
        RenderEntity::update(dt);
        if (visible_) {
            remaining_ = std::max(0.0f, remaining_ - dt);
        }
    }

    void Banner::draw() {
        RenderEntity::draw();
        if (!visible_)
            return;

        rq().submitUI([this] {
            const int fontSize = 32;
            const char* text = text_.empty() ? "Ready" : text_.c_str();
            const int textW = MeasureText(text, fontSize);
            const int x = g_cfg.viewPortWidth / 2 - textW / 2;
            const int y = g_cfg.viewPortHeight / 2 - fontSize / 2;
            DrawRectangle(x - 12, y - 8, textW + 24, fontSize + 16, Fade(BLACK, 0.6f));
            DrawText(text, x, y, fontSize, WHITE);
        });
    }
} // namespace breakout
