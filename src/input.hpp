#pragma once
#include <string>
#include <unordered_map>

#include "raylib.h"

namespace rlge {
    class Input {
    public:
        void bind(const std::string& action, int key) {
            keys_[action] = key;
        }

        bool down(const std::string& action) const {
            auto it = keys_.find(action);
            if (it == keys_.end())
                return false;
            return IsKeyDown(it->second);
        }

        bool pressed(const std::string& action) const {
            const auto it = keys_.find(action);
            if (it == keys_.end())
                return false;
            return IsKeyPressed(it->second);
        }

    private:
        std::unordered_map<std::string, int> keys_;
    };

}
