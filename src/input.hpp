#pragma once
#include <string>
#include <unordered_map>

namespace rlge {
    class Input {
    public:
        void bind(const std::string& action, int key);

        bool down(const std::string& action) const;

        bool pressed(const std::string& action) const;

    private:
        std::unordered_map<std::string, int> keys_;
    };

}
