module;

#include <array>
#include <cstddef>

export module engine.input;
import engine.core;

export namespace engine
{

  enum class Key : u16
  {
    Unknown = 0,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,
    Up,
    Down,
    Left,
    Right,
    Count,
  };

  enum class MouseButton : u8
  {
    Left = 0,
    Right,
    Middle,
    Count,
  };

  // Level state (is_pressed) plus one frame of history (key_down_previous)
  // is enough to derive edges (just_pressed/just_released) without any
  // event queue - backends only ever need to fill in the "current" arrays
  // and copy them into "previous" once per frame, before polling.
  struct InputState
  {
    // Public data fields are intentional (same pattern as Window): backends
    // fill them directly each frame and the query methods only read them.
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    std::array<bool, static_cast<std::size_t>(Key::Count)> key_down{};
    std::array<bool, static_cast<std::size_t>(Key::Count)> key_down_previous{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_down{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_down_previous{};

    f32 mouse_x       = 0.0F;
    f32 mouse_y       = 0.0F;
    f32 mouse_delta_x = 0.0F;
    f32 mouse_delta_y = 0.0F;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    [[nodiscard]] auto is_pressed(Key key) const -> bool { return key_down.at(static_cast<std::size_t>(key)); }

    [[nodiscard]] auto just_pressed(Key key) const -> bool
    {
      auto index = static_cast<std::size_t>(key);
      return key_down.at(index) && !key_down_previous.at(index);
    }

    [[nodiscard]] auto just_released(Key key) const -> bool
    {
      auto index = static_cast<std::size_t>(key);
      return !key_down.at(index) && key_down_previous.at(index);
    }

    [[nodiscard]] auto is_pressed(MouseButton button) const -> bool
    {
      return mouse_down.at(static_cast<std::size_t>(button));
    }

    [[nodiscard]] auto just_pressed(MouseButton button) const -> bool
    {
      auto index = static_cast<std::size_t>(button);
      return mouse_down.at(index) && !mouse_down_previous.at(index);
    }

    [[nodiscard]] auto just_released(MouseButton button) const -> bool
    {
      auto index = static_cast<std::size_t>(button);
      return !mouse_down.at(index) && mouse_down_previous.at(index);
    }
  };

} // namespace engine
