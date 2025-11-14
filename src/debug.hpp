  #pragma once

namespace rlge {

    struct HasDebugOverlay {
        virtual ~HasDebugOverlay() = default;
        virtual void debugOverlay() = 0;
    };

} // namespace rlge
