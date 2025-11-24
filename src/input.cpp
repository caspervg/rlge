#include "input.hpp"

// This file is intentionally mostly empty.
// The Input class is now a template with all implementation in the header (input.hpp).
// This allows users to define their own Action enum types.
// 
// Example usage with custom actions:
//
// enum class MyGameAction {
//     Shoot,
//     Reload,
//     Sprint
// };
//
// rlge::Input<MyGameAction> input;
// input.bind(MyGameAction::Shoot, rlge::KeyCode::Space);

