#include "input.hpp"

namespace rlge {
    
    void Input::bind(Action action, KeyCode key) {
        keyBindings_[action] = KeyBinding{key};
    }

    void Input::bindMouse(Action action, MouseButton button) {
        mouseBindings_[action] = MouseBinding{button};
    }

    void Input::bindGamepad(Action action, int gamepadId, GamepadButton button) {
        gamepadBindings_[action] = GamepadBinding{gamepadId, button};
    }

    void Input::bindAxis(Action action, KeyCode negative, KeyCode positive) {
        auto& binding = axisBindings_[action];
        binding.keyBinding = AxisKeyBinding{negative, positive};
    }

    void Input::bindAxis(Action action, int gamepadId, GamepadAxis axis) {
        auto& binding = axisBindings_[action];
        binding.gamepadBinding = AxisGamepadBinding{gamepadId, axis};
    }

    void Input::setAxisDeadZone(Action action, float deadZone) {
        axisBindings_[action].deadZone = deadZone;
    }

    bool Input::down(Action action) const {
        const auto it = keyBindings_.find(action);
        if (it == keyBindings_.end())
            return false;
        return IsKeyDown(static_cast<int>(it->second.key));
    }

    bool Input::pressed(Action action) const {
        const auto it = keyBindings_.find(action);
        if (it == keyBindings_.end())
            return false;
        return IsKeyPressed(static_cast<int>(it->second.key));
    }

    bool Input::released(Action action) const {
        const auto it = keyBindings_.find(action);
        if (it == keyBindings_.end())
            return false;
        return IsKeyReleased(static_cast<int>(it->second.key));
    }

    bool Input::mouseDown(Action action) const {
        const auto it = mouseBindings_.find(action);
        if (it == mouseBindings_.end())
            return false;
        return IsMouseButtonDown(static_cast<int>(it->second.button));
    }

    bool Input::mousePressed(Action action) const {
        const auto it = mouseBindings_.find(action);
        if (it == mouseBindings_.end())
            return false;
        return IsMouseButtonPressed(static_cast<int>(it->second.button));
    }

    Vector2 Input::mousePosition() const {
        return GetMousePosition();
    }

    bool Input::gamepadDown(Action action) const {
        const auto it = gamepadBindings_.find(action);
        if (it == gamepadBindings_.end())
            return false;
        
        if (!IsGamepadAvailable(it->second.gamepadId))
            return false;
        
        return IsGamepadButtonDown(it->second.gamepadId, toRaylibGamepadButton(it->second.button));
    }

    bool Input::gamepadPressed(Action action) const {
        const auto it = gamepadBindings_.find(action);
        if (it == gamepadBindings_.end())
            return false;
        
        if (!IsGamepadAvailable(it->second.gamepadId))
            return false;
        
        return IsGamepadButtonPressed(it->second.gamepadId, toRaylibGamepadButton(it->second.button));
    }

    float Input::axisValue(Action action) const {
        const auto it = axisBindings_.find(action);
        if (it == axisBindings_.end())
            return 0.0f;

        const auto& binding = it->second;
        float value = 0.0f;

        // Check key binding first
        if (binding.keyBinding.has_value()) {
            const auto& keyBinding = binding.keyBinding.value();
            if (IsKeyDown(static_cast<int>(keyBinding.negative))) {
                value -= 1.0f;
            }
            if (IsKeyDown(static_cast<int>(keyBinding.positive))) {
                value += 1.0f;
            }
        }

        // Check gamepad binding (overrides key binding if both exist)
        if (binding.gamepadBinding.has_value()) {
            const auto& gamepadBinding = binding.gamepadBinding.value();
            if (IsGamepadAvailable(gamepadBinding.gamepadId)) {
                float axisVal = GetGamepadAxisMovement(
                    gamepadBinding.gamepadId,
                    toRaylibGamepadAxis(gamepadBinding.axis)
                );
                
                // Apply dead zone
                if (std::abs(axisVal) > binding.deadZone) {
                    value = axisVal;
                }
            }
        }

        return value;
    }

    int Input::toRaylibGamepadButton(GamepadButton button) const {
        switch (button) {
            case GamepadButton::A: return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
            case GamepadButton::B: return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
            case GamepadButton::X: return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
            case GamepadButton::Y: return GAMEPAD_BUTTON_RIGHT_FACE_UP;
            case GamepadButton::LeftBumper: return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
            case GamepadButton::RightBumper: return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
            case GamepadButton::Back: return GAMEPAD_BUTTON_MIDDLE_LEFT;
            case GamepadButton::Start: return GAMEPAD_BUTTON_MIDDLE_RIGHT;
            case GamepadButton::LeftThumb: return GAMEPAD_BUTTON_LEFT_THUMB;
            case GamepadButton::RightThumb: return GAMEPAD_BUTTON_RIGHT_THUMB;
            case GamepadButton::DPadUp: return GAMEPAD_BUTTON_LEFT_FACE_UP;
            case GamepadButton::DPadRight: return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
            case GamepadButton::DPadDown: return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
            case GamepadButton::DPadLeft: return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
            default: return GAMEPAD_BUTTON_UNKNOWN;
        }
    }

    int Input::toRaylibGamepadAxis(GamepadAxis axis) const {
        switch (axis) {
            case GamepadAxis::LeftX: return GAMEPAD_AXIS_LEFT_X;
            case GamepadAxis::LeftY: return GAMEPAD_AXIS_LEFT_Y;
            case GamepadAxis::RightX: return GAMEPAD_AXIS_RIGHT_X;
            case GamepadAxis::RightY: return GAMEPAD_AXIS_RIGHT_Y;
            case GamepadAxis::LeftTrigger: return GAMEPAD_AXIS_LEFT_TRIGGER;
            case GamepadAxis::RightTrigger: return GAMEPAD_AXIS_RIGHT_TRIGGER;
            default: return GAMEPAD_AXIS_LEFT_X;
        }
    }

}

