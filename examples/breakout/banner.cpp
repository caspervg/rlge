#include "banner.hpp"

#include "breakout_config.hpp"
#include <algorithm>
#include <utility>
#include "raylib.h"
#include "runtime.hpp"
#include "window.hpp"

namespace breakout {
    using namespace rlge;

    namespace {
        std::pair<Rectangle, float> uiFrame(const Scene& scene) {
            Rectangle view{0.0f, 0.0f, static_cast<float>(GetRenderWidth()), static_cast<float>(GetRenderHeight())};
            if (const auto* primary = scene.runtime().primaryView()) {
                view = primary->viewport;
            }

            const Vector2 dpi = scene.runtime().window().dpiScale();
            const float dpiScale = std::max(dpi.x, dpi.y);
            const float viewportScale = view.height > 0.0f ? view.height / g_cfg.viewPortHeight : 1.0f;
            return {view, dpiScale * viewportScale};
        }
    }

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
            const auto [view, scale] = uiFrame(scene());
            const int fontSize = std::max(20, static_cast<int>(32 * scale));
            const char* text = text_.empty() ? "Ready" : text_.c_str();
            const int textW = MeasureText(text, fontSize);
            const int x = static_cast<int>(view.x + view.width / 2 - textW / 2);
            const int y = static_cast<int>(view.y + view.height / 2 - fontSize / 2);
            const int paddingX = static_cast<int>(12 * scale);
            const int paddingY = static_cast<int>(8 * scale);
            DrawRectangle(x - paddingX, y - paddingY, textW + paddingX * 2, fontSize + paddingY * 2, Fade(BLACK, 0.6f));
            DrawText(text, x, y, fontSize, WHITE);
        });
    }
} // namespace breakout
