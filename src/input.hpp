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

    // Input class with type-safe bindings
    class Input {
    public:
        // Keyboard binding
        void bind(Action action, KeyCode key);

        // Mouse binding
        void bindMouse(Action action, MouseButton button);

        // Gamepad binding
        void bindGamepad(Action action, int gamepadId, GamepadButton button);

        // Axis binding for key pairs (e.g., A/D for horizontal movement)
        void bindAxis(Action action, KeyCode negative, KeyCode positive);

        // Axis binding for gamepad axis
        void bindAxis(Action action, int gamepadId, GamepadAxis axis);

        // Set dead zone for an axis
        void setAxisDeadZone(Action action, float deadZone);

        // Query if action button is currently held down
        bool down(Action action) const;

        // Query if action button was just pressed this frame
        bool pressed(Action action) const;

        // Query if action button was just released this frame
        bool released(Action action) const;

        // Query if mouse button for action is down
        bool mouseDown(Action action) const;

        // Query if mouse button for action was just pressed
        bool mousePressed(Action action) const;

        // Get mouse position
        Vector2 mousePosition() const;

        // Query if gamepad button for action is down
        bool gamepadDown(Action action) const;

        // Query if gamepad button for action was just pressed
        bool gamepadPressed(Action action) const;

        // Get axis value for action (-1.0 to 1.0)
        float axisValue(Action action) const;

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

        std::unordered_map<Action, KeyBinding> keyBindings_;
        std::unordered_map<Action, MouseBinding> mouseBindings_;
        std::unordered_map<Action, GamepadBinding> gamepadBindings_;
        std::unordered_map<Action, AxisBinding> axisBindings_;

        // Helper to convert GamepadButton to Raylib constant
        int toRaylibGamepadButton(GamepadButton button) const;

        // Helper to convert GamepadAxis to Raylib constant
        int toRaylibGamepadAxis(GamepadAxis axis) const;
    };

}
