#pragma once

#define RLGE_USE_CUSTOM_ACTION

namespace anim_demo {
    enum class Action {
        MoveLeft,
        MoveRight,
        Attack,
        Hurt,
        Death,
        Reset,
        VariantPrev,
        VariantNext
    };
}

// Inject anim_demo::Action into the rlge namespace for Input<>
namespace rlge {
    using Action = anim_demo::Action;
}
