#pragma once
#include <unordered_map>
#include <optional>
#include <cmath>
#include "raylib.h"

namespace rlge {
    // KeyCode enum wrapping Raylib key constants
    enum class KeyCode {
        Null = KEY_NULL,
        Apostrophe = KEY_APOSTROPHE,
        Comma = KEY_COMMA,
        Minus = KEY_MINUS,
        Period = KEY_PERIOD,
        Slash = KEY_SLASH,
        Zero = KEY_ZERO,
        One = KEY_ONE,
        Two = KEY_TWO,
        Three = KEY_THREE,
        Four = KEY_FOUR,
        Five = KEY_FIVE,
        Six = KEY_SIX,
        Seven = KEY_SEVEN,
        Eight = KEY_EIGHT,
        Nine = KEY_NINE,
        Semicolon = KEY_SEMICOLON,
        Equal = KEY_EQUAL,
        A = KEY_A,
        B = KEY_B,
        C = KEY_C,
        D = KEY_D,
        E = KEY_E,
        F = KEY_F,
        G = KEY_G,
        H = KEY_H,
        I = KEY_I,
        J = KEY_J,
        K = KEY_K,
        L = KEY_L,
        M = KEY_M,
        N = KEY_N,
        O = KEY_O,
        P = KEY_P,
        Q = KEY_Q,
        R = KEY_R,
        S = KEY_S,
        T = KEY_T,
        U = KEY_U,
        V = KEY_V,
        W = KEY_W,
        X = KEY_X,
        Y = KEY_Y,
        Z = KEY_Z,
        LeftBracket = KEY_LEFT_BRACKET,
        Backslash = KEY_BACKSLASH,
        RightBracket = KEY_RIGHT_BRACKET,
        Grave = KEY_GRAVE,
        Space = KEY_SPACE,
        Escape = KEY_ESCAPE,
        Enter = KEY_ENTER,
        Tab = KEY_TAB,
        Backspace = KEY_BACKSPACE,
        Insert = KEY_INSERT,
        Delete = KEY_DELETE,
        Right = KEY_RIGHT,
        Left = KEY_LEFT,
        Down = KEY_DOWN,
        Up = KEY_UP,
        PageUp = KEY_PAGE_UP,
        PageDown = KEY_PAGE_DOWN,
        Home = KEY_HOME,
        End = KEY_END,
        CapsLock = KEY_CAPS_LOCK,
        ScrollLock = KEY_SCROLL_LOCK,
        NumLock = KEY_NUM_LOCK,
        PrintScreen = KEY_PRINT_SCREEN,
        Pause = KEY_PAUSE,
        F1 = KEY_F1,
        F2 = KEY_F2,
        F3 = KEY_F3,
        F4 = KEY_F4,
        F5 = KEY_F5,
        F6 = KEY_F6,
        F7 = KEY_F7,
        F8 = KEY_F8,
        F9 = KEY_F9,
        F10 = KEY_F10,
        F11 = KEY_F11,
        F12 = KEY_F12,
        LeftShift = KEY_LEFT_SHIFT,
        LeftControl = KEY_LEFT_CONTROL,
        LeftAlt = KEY_LEFT_ALT,
        LeftSuper = KEY_LEFT_SUPER,
        RightShift = KEY_RIGHT_SHIFT,
        RightControl = KEY_RIGHT_CONTROL,
        RightAlt = KEY_RIGHT_ALT,
        RightSuper = KEY_RIGHT_SUPER,
        KbMenu = KEY_KB_MENU,
        Kp0 = KEY_KP_0,
        Kp1 = KEY_KP_1,
        Kp2 = KEY_KP_2,
        Kp3 = KEY_KP_3,
        Kp4 = KEY_KP_4,
        Kp5 = KEY_KP_5,
        Kp6 = KEY_KP_6,
        Kp7 = KEY_KP_7,
        Kp8 = KEY_KP_8,
        Kp9 = KEY_KP_9,
        KpDecimal = KEY_KP_DECIMAL,
        KpDivide = KEY_KP_DIVIDE,
        KpMultiply = KEY_KP_MULTIPLY,
        KpSubtract = KEY_KP_SUBTRACT,
        KpAdd = KEY_KP_ADD,
        KpEnter = KEY_KP_ENTER,
        KpEqual = KEY_KP_EQUAL
    };

    // Default Action enum - users can define their own in their code
    enum class Action {
        MoveLeft,
        MoveRight,
        MoveUp,
        MoveDown,
        Jump,
        Fire,
        Interact,
        Menu,
        Confirm,
        Cancel
    };

    // Mouse button enum
    enum class MouseButton {
        Left = MOUSE_BUTTON_LEFT,
        Right = MOUSE_BUTTON_RIGHT,
        Middle = MOUSE_BUTTON_MIDDLE
    };

    // Gamepad button enum
    enum class GamepadButton {
        A,
        B,
        X,
        Y,
        LeftBumper,
        RightBumper,
        Back,
        Start,
        LeftThumb,
        RightThumb,
        DPadLeft,
        DPadRight,
        DPadUp,
        DPadDown
    };

    // Gamepad axis enum
    enum class GamepadAxis {
        LeftX,
        LeftY,
        RightX,
        RightY,
        LeftTrigger,
        RightTrigger
    };

    // Template Input class - allows users to define their own Action enum
    template<typename ActionEnum = Action>
    class Input {
    public:
        // Keyboard binding
        void bind(ActionEnum action, KeyCode key) {
            keyBindings_[action] = KeyBinding{key};
        }

        // Mouse binding
        void bindMouse(ActionEnum action, MouseButton button) {
            mouseBindings_[action] = MouseBinding{button};
        }

        // Gamepad binding
        void bindGamepad(ActionEnum action, int gamepadId, GamepadButton button) {
            gamepadBindings_[action] = GamepadBinding{gamepadId, button};
        }

        // Axis binding for key pairs (e.g., A/D for horizontal movement)
        void bindAxis(ActionEnum action, KeyCode negative, KeyCode positive) {
            auto& binding = axisBindings_[action];
            binding.keyBinding = AxisKeyBinding{negative, positive};
        }

        // Axis binding for gamepad axis
        void bindAxis(ActionEnum action, int gamepadId, GamepadAxis axis) {
            auto& binding = axisBindings_[action];
            binding.gamepadBinding = AxisGamepadBinding{gamepadId, axis};
        }

        // Set dead zone for an axis
        void setAxisDeadZone(ActionEnum action, float deadZone) {
            axisBindings_[action].deadZone = deadZone;
        }

        // Query if action button is currently held down
        bool down(ActionEnum action) const {
            const auto it = keyBindings_.find(action);
            if (it == keyBindings_.end())
                return false;
            return IsKeyDown(static_cast<int>(it->second.key));
        }

        // Query if action button was just pressed this frame
        bool pressed(ActionEnum action) const {
            const auto it = keyBindings_.find(action);
            if (it == keyBindings_.end())
                return false;
            return IsKeyPressed(static_cast<int>(it->second.key));
        }

        // Query if action button was just released this frame
        bool released(ActionEnum action) const {
            const auto it = keyBindings_.find(action);
            if (it == keyBindings_.end())
                return false;
            return IsKeyReleased(static_cast<int>(it->second.key));
        }

        // Query if mouse button for action is down
        bool mouseDown(ActionEnum action) const {
            const auto it = mouseBindings_.find(action);
            if (it == mouseBindings_.end())
                return false;
            return IsMouseButtonDown(static_cast<int>(it->second.button));
        }

        // Query if mouse button for action was just pressed
        bool mousePressed(ActionEnum action) const {
            const auto it = mouseBindings_.find(action);
            if (it == mouseBindings_.end())
                return false;
            return IsMouseButtonPressed(static_cast<int>(it->second.button));
        }

        // Get mouse position
        Vector2 mousePosition() const {
            return GetMousePosition();
        }

        // Query if gamepad button for action is down
        bool gamepadDown(ActionEnum action) const {
            const auto it = gamepadBindings_.find(action);
            if (it == gamepadBindings_.end())
                return false;
            
            if (!IsGamepadAvailable(it->second.gamepadId))
                return false;
            
            return IsGamepadButtonDown(it->second.gamepadId, toRaylibGamepadButton(it->second.button));
        }

        // Query if gamepad button for action was just pressed
        bool gamepadPressed(ActionEnum action) const {
            const auto it = gamepadBindings_.find(action);
            if (it == gamepadBindings_.end())
                return false;
            
            if (!IsGamepadAvailable(it->second.gamepadId))
                return false;
            
            return IsGamepadButtonPressed(it->second.gamepadId, toRaylibGamepadButton(it->second.button));
        }

        // Get axis value for action (-1.0 to 1.0)
        float axisValue(ActionEnum action) const {
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

            // Check gamepad binding (only overrides if axis exceeds dead zone)
            if (binding.gamepadBinding.has_value()) {
                const auto& gamepadBinding = binding.gamepadBinding.value();
                if (IsGamepadAvailable(gamepadBinding.gamepadId)) {
                    float axisVal = GetGamepadAxisMovement(
                        gamepadBinding.gamepadId,
                        toRaylibGamepadAxis(gamepadBinding.axis)
                    );
                    
                    // Only use gamepad value if it exceeds dead zone
                    if (std::abs(axisVal) > binding.deadZone) {
                        value = axisVal;
                    }
                }
            }

            return value;
        }

    private:
        struct KeyBinding {
            KeyCode key;
        };

        struct MouseBinding {
            MouseButton button;
        };

        struct GamepadBinding {
            int gamepadId;
            GamepadButton button;
        };

        struct AxisKeyBinding {
            KeyCode negative;
            KeyCode positive;
        };

        struct AxisGamepadBinding {
            int gamepadId;
            GamepadAxis axis;
        };

        struct AxisBinding {
            std::optional<AxisKeyBinding> keyBinding;
            std::optional<AxisGamepadBinding> gamepadBinding;
            float deadZone = 0.1f;
        };

        std::unordered_map<ActionEnum, KeyBinding> keyBindings_;
        std::unordered_map<ActionEnum, MouseBinding> mouseBindings_;
        std::unordered_map<ActionEnum, GamepadBinding> gamepadBindings_;
        std::unordered_map<ActionEnum, AxisBinding> axisBindings_;

        // Helper to convert GamepadButton to Raylib constant
        int toRaylibGamepadButton(GamepadButton button) const {
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

        // Helper to convert GamepadAxis to Raylib constant
        int toRaylibGamepadAxis(GamepadAxis axis) const {
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
    };

    // Type alias using the default Action enum for convenience
    using DefaultInput = Input<Action>;

}
