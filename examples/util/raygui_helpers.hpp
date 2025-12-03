#pragma once

#include <string>
#include <functional>

#include "raygui.h"
#include "render_queue.hpp"

namespace rlge::ui {

    // Load a raygui style file from disk (e.g., exported by rGuiStyler).
    // Pass a path relative to working dir (e.g., "assets/ui/theme.rgs").
    inline void loadStyle(const std::string& path) {
        GuiLoadStyle(path.c_str());
    }

    // Optional: set a few common style overrides.
    inline void applyBaseStyle(const int textSize = 18, const int textAlign = TEXT_ALIGN_LEFT) {
        GuiSetStyle(DEFAULT, TEXT_SIZE, textSize);
        GuiSetStyle(LABEL, TEXT_ALIGNMENT, textAlign);
    }

    // Lock/unlock raygui input (useful to pause raygui when ImGui is active).
    inline void setEnabled(const bool enabled) {
        if (enabled) {
            GuiUnlock();
            GuiSetState(STATE_NORMAL);
        } else {
            GuiLock();
        }
    }

    // Submit a raygui block through the render queue UI channel.
    inline void submit(RenderQueue& rq, std::function<void()> fn) {
        rq.submitUI(std::move(fn));
    }

} // namespace rlge::ui
