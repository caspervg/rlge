#include "input.hpp"

#include "raylib.h"

namespace rlge {
    void Input::bind(const std::string& action, int key) {
        keys_[action] = key;
    }

    bool Input::down(const std::string& action) const {
        const auto it = keys_.find(action);
        if (it == keys_.end())
            return false;
        return IsKeyDown(it->second);
    }

    bool Input::pressed(const std::string& action) const {
        const auto it = keys_.find(action);
        if (it == keys_.end())
            return false;
        return IsKeyPressed(it->second);
    }
}

