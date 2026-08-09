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

  class InputState
  {
  public:
    [[nodiscard]] auto is_pressed(Key key) const -> bool { return key_down_.at(static_cast<std::size_t>(key)); }

    [[nodiscard]] auto just_pressed(Key key) const -> bool
    {
      auto index = static_cast<std::size_t>(key);
      return key_down_.at(index) && !key_down_previous_.at(index);
    }

    [[nodiscard]] auto just_released(Key key) const -> bool
    {
      auto index = static_cast<std::size_t>(key);
      return !key_down_.at(index) && key_down_previous_.at(index);
    }

    [[nodiscard]] auto is_pressed(MouseButton button) const -> bool
    {
      return mouse_down_.at(static_cast<std::size_t>(button));
    }

    [[nodiscard]] auto just_pressed(MouseButton button) const -> bool
    {
      auto index = static_cast<std::size_t>(button);
      return mouse_down_.at(index) && !mouse_down_previous_.at(index);
    }

    [[nodiscard]] auto just_released(MouseButton button) const -> bool
    {
      auto index = static_cast<std::size_t>(button);
      return !mouse_down_.at(index) && mouse_down_previous_.at(index);
    }

    [[nodiscard]] auto mouse_x() const -> f32 { return mouse_x_; }
    [[nodiscard]] auto mouse_y() const -> f32 { return mouse_y_; }
    [[nodiscard]] auto mouse_delta_x() const -> f32 { return mouse_delta_x_; }
    [[nodiscard]] auto mouse_delta_y() const -> f32 { return mouse_delta_y_; }

    // Advance to the next frame, then report the current hardware state.
    // Called by input plugins once per frame.
    void begin_frame()
    {
      key_down_previous_   = key_down_;
      mouse_down_previous_ = mouse_down_;
    }

    void set_key_down(Key key, bool down) { key_down_.at(static_cast<std::size_t>(key)) = down; }

    void set_mouse_button(MouseButton button, bool down) { mouse_down_.at(static_cast<std::size_t>(button)) = down; }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void set_mouse_position(f32 x, f32 y)
    {
      mouse_delta_x_ = x - mouse_x_;
      mouse_delta_y_ = y - mouse_y_;
      mouse_x_       = x;
      mouse_y_       = y;
    }

  private:
    std::array<bool, static_cast<std::size_t>(Key::Count)> key_down_{};
    std::array<bool, static_cast<std::size_t>(Key::Count)> key_down_previous_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_down_{};
    std::array<bool, static_cast<std::size_t>(MouseButton::Count)> mouse_down_previous_{};

    f32 mouse_x_       = 0.0F;
    f32 mouse_y_       = 0.0F;
    f32 mouse_delta_x_ = 0.0F;
    f32 mouse_delta_y_ = 0.0F;
  };

} // namespace engine
