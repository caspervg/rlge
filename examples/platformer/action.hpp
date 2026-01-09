#pragma once

#define RLGE_USE_CUSTOM_ACTION

namespace platformer {
    enum class Action {
        MoveLeft,
        MoveRight,
        Jump
    };
}

// Automatically inject platformer::Action into rlge namespace
// This makes Input<> default to platformer::Action in any file that includes this header
namespace rlge {
    using Action = platformer::Action;
}
