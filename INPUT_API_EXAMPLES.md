# Enhanced Input API Examples

This document demonstrates the new type-safe Input API with support for keyboard, mouse, gamepad, and axis bindings.

## Basic Keyboard Input

```cpp
#include "input.hpp"

rlge::Input input;

// Bind actions to keys using type-safe enums
input.bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
input.bind(rlge::Action::MoveRight, rlge::KeyCode::D);
input.bind(rlge::Action::Jump, rlge::KeyCode::Space);
input.bind(rlge::Action::Fire, rlge::KeyCode::LeftControl);

// Query input state
if (input.down(rlge::Action::MoveLeft)) {
    // Move left while key is held
}

if (input.pressed(rlge::Action::Jump)) {
    // Jump when key is first pressed
}

if (input.released(rlge::Action::Fire)) {
    // Stop firing when key is released
}
```

## Mouse Input

```cpp
// Bind mouse buttons to actions
input.bindMouse(rlge::Action::Fire, rlge::MouseButton::Left);
input.bindMouse(rlge::Action::Interact, rlge::MouseButton::Right);

// Query mouse button state
if (input.mouseDown(rlge::Action::Fire)) {
    // Fire while mouse button is held
}

if (input.mousePressed(rlge::Action::Interact)) {
    // Interact when mouse button is first pressed
}

// Get mouse position
Vector2 mousePos = input.mousePosition();
```

## Gamepad Input

```cpp
// Bind gamepad buttons (gamepad ID 0 = first gamepad)
input.bindGamepad(rlge::Action::Jump, 0, rlge::GamepadButton::A);
input.bindGamepad(rlge::Action::Fire, 0, rlge::GamepadButton::X);
input.bindGamepad(rlge::Action::Menu, 0, rlge::GamepadButton::Start);

// Query gamepad button state
if (input.gamepadDown(rlge::Action::Jump)) {
    // Jump while gamepad button is held
}

if (input.gamepadPressed(rlge::Action::Fire)) {
    // Fire when gamepad button is first pressed
}
```

## Axis Input (Keyboard)

```cpp
// Bind axis to key pairs (negative/positive)
// For horizontal movement: A (negative) and D (positive)
input.bindAxis(rlge::Action::MoveLeft, rlge::KeyCode::A, rlge::KeyCode::D);

// For vertical movement: W (negative) and S (positive)
input.bindAxis(rlge::Action::MoveUp, rlge::KeyCode::W, rlge::KeyCode::S);

// Get axis value (-1.0 to 1.0)
float horizontal = input.axisValue(rlge::Action::MoveLeft);
float vertical = input.axisValue(rlge::Action::MoveUp);

// Use for smooth movement
playerPosition.x += horizontal * speed * deltaTime;
playerPosition.y += vertical * speed * deltaTime;
```

## Axis Input (Gamepad)

```cpp
// Bind axis to gamepad analog sticks
input.bindAxis(rlge::Action::MoveLeft, 0, rlge::GamepadAxis::LeftX);
input.bindAxis(rlge::Action::MoveUp, 0, rlge::GamepadAxis::LeftY);

// Set dead zone to prevent drift (default is 0.1)
input.setAxisDeadZone(rlge::Action::MoveLeft, 0.2f);
input.setAxisDeadZone(rlge::Action::MoveUp, 0.2f);

// Get axis value with dead zone applied
float horizontal = input.axisValue(rlge::Action::MoveLeft);
float vertical = input.axisValue(rlge::Action::MoveUp);
```

## Using Custom Actions

The Input class is a template that allows you to define your own Action enum for game-specific actions:

```cpp
// Define your own action enum
enum class MyGameAction {
    Shoot,
    Reload,
    Sprint,
    UseItem,
    ToggleInventory
};

// Use it with the Input template
rlge::Input<MyGameAction> customInput;

// Bind custom actions
customInput.bind(MyGameAction::Shoot, rlge::KeyCode::Space);
customInput.bind(MyGameAction::Reload, rlge::KeyCode::R);
customInput.bind(MyGameAction::Sprint, rlge::KeyCode::LeftShift);

// Use in gameplay
if (customInput.down(MyGameAction::Shoot)) {
    fireBullet();
}
if (customInput.pressed(MyGameAction::Reload)) {
    reloadWeapon();
}
```

However, for simplicity in a learning context, the default Action enum (used by `Input<>` or `Input<Action>`) provides common actions:

- `Action::MoveLeft`
- `Action::MoveRight`
- `Action::MoveUp`
- `Action::MoveDown`
- `Action::Jump`
- `Action::Fire`
- `Action::Interact`
- `Action::Menu`
- `Action::Confirm`
- `Action::Cancel`

The engine uses `Input<>` (equivalent to `Input<Action>`) by default in Runtime and Scene.

## Complete Example Scene

```cpp
class PlayerEntity : public rlge::RenderEntity {
public:
    void update(float dt) override {
        const auto& input = scene().input();
        auto* transform = get<rlge::Transform>();
        if (!transform) return;

        // Keyboard movement
        float horizontal = 0.0f;
        float vertical = 0.0f;

        if (input.down(rlge::Action::MoveLeft))
            horizontal -= 1.0f;
        if (input.down(rlge::Action::MoveRight))
            horizontal += 1.0f;
        if (input.down(rlge::Action::MoveUp))
            vertical -= 1.0f;
        if (input.down(rlge::Action::MoveDown))
            vertical += 1.0f;

        // Or use axis input for smoother control
        // horizontal = input.axisValue(rlge::Action::MoveLeft);
        // vertical = input.axisValue(rlge::Action::MoveUp);

        transform->position.x += horizontal * speed * dt;
        transform->position.y += vertical * speed * dt;

        // Jump action
        if (input.pressed(rlge::Action::Jump)) {
            jump();
        }

        // Mouse shooting
        if (input.mousePressed(rlge::Action::Fire)) {
            Vector2 target = input.mousePosition();
            shootAt(target);
        }
    }

private:
    float speed = 200.0f;
};

// In your main function or scene setup:
void setupInput(rlge::Runtime& runtime) {
    auto& input = runtime.input();

    // Keyboard bindings
    input.bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
    input.bind(rlge::Action::MoveRight, rlge::KeyCode::D);
    input.bind(rlge::Action::MoveUp, rlge::KeyCode::W);
    input.bind(rlge::Action::MoveDown, rlge::KeyCode::S);
    input.bind(rlge::Action::Jump, rlge::KeyCode::Space);

    // Mouse bindings
    input.bindMouse(rlge::Action::Fire, rlge::MouseButton::Left);
    input.bindMouse(rlge::Action::Interact, rlge::MouseButton::Right);

    // Gamepad bindings (optional)
    input.bindGamepad(rlge::Action::Jump, 0, rlge::GamepadButton::A);
    input.bindGamepad(rlge::Action::Fire, 0, rlge::GamepadButton::X);

    // Axis bindings for smooth analog movement
    input.bindAxis(rlge::Action::MoveLeft, 0, rlge::GamepadAxis::LeftX);
    input.bindAxis(rlge::Action::MoveUp, 0, rlge::GamepadAxis::LeftY);
    input.setAxisDeadZone(rlge::Action::MoveLeft, 0.15f);
    input.setAxisDeadZone(rlge::Action::MoveUp, 0.15f);
}
```

## Available KeyCodes

The `KeyCode` enum includes all Raylib key constants:
- Letter keys: `KeyCode::A` through `KeyCode::Z`
- Number keys: `KeyCode::Zero` through `KeyCode::Nine`
- Arrow keys: `KeyCode::Up`, `KeyCode::Down`, `KeyCode::Left`, `KeyCode::Right`
- Function keys: `KeyCode::F1` through `KeyCode::F12`
- Special keys: `KeyCode::Space`, `KeyCode::Enter`, `KeyCode::Escape`, `KeyCode::Tab`, etc.
- Modifier keys: `KeyCode::LeftShift`, `KeyCode::LeftControl`, `KeyCode::LeftAlt`, etc.
- Keypad: `KeyCode::Kp0` through `KeyCode::Kp9`, `KeyCode::KpAdd`, etc.

## Available Gamepad Buttons

- `GamepadButton::A`, `GamepadButton::B`, `GamepadButton::X`, `GamepadButton::Y`
- `GamepadButton::LeftBumper`, `GamepadButton::RightBumper`
- `GamepadButton::Back`, `GamepadButton::Start`
- `GamepadButton::LeftThumb`, `GamepadButton::RightThumb`
- `GamepadButton::DPadUp`, `GamepadButton::DPadDown`, `GamepadButton::DPadLeft`, `GamepadButton::DPadRight`

## Available Gamepad Axes

- `GamepadAxis::LeftX`, `GamepadAxis::LeftY` - Left analog stick
- `GamepadAxis::RightX`, `GamepadAxis::RightY` - Right analog stick
- `GamepadAxis::LeftTrigger`, `GamepadAxis::RightTrigger` - Analog triggers

## Migration from Old API

Old string-based API:
```cpp
input.bind("move_left", KEY_A);
if (input.down("move_left")) { ... }
```

New type-safe API:
```cpp
input.bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
if (input.down(rlge::Action::MoveLeft)) { ... }
```

Benefits of the new API:
1. **Type safety**: Compile-time checking prevents typos
2. **IDE support**: Auto-completion for actions and keys
3. **Refactoring**: Easy to rename actions across the codebase
4. **More features**: Mouse, gamepad, and axis support
5. **Educational**: Clear, self-documenting code for learners
