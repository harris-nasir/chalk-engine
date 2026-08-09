module;

#include <SDL3/SDL.h>
#include <windows.h>

export module engine.input.sdl3;

import engine.core;
import engine.input;
import engine.platform;

namespace engine
{

  // SDL_Scancode is layout-independent (physical key position), matching
  // how Key is meant to be interpreted - same physical mapping as the
  // Win32 VK_* fallback below.
  [[nodiscard]] auto to_sdl_scancode(Key key) -> SDL_Scancode
  {
    switch (key)
    {
      case Key::A:
        return SDL_SCANCODE_A;
      case Key::B:
        return SDL_SCANCODE_B;
      case Key::C:
        return SDL_SCANCODE_C;
      case Key::D:
        return SDL_SCANCODE_D;
      case Key::E:
        return SDL_SCANCODE_E;
      case Key::F:
        return SDL_SCANCODE_F;
      case Key::G:
        return SDL_SCANCODE_G;
      case Key::H:
        return SDL_SCANCODE_H;
      case Key::I:
        return SDL_SCANCODE_I;
      case Key::J:
        return SDL_SCANCODE_J;
      case Key::K:
        return SDL_SCANCODE_K;
      case Key::L:
        return SDL_SCANCODE_L;
      case Key::M:
        return SDL_SCANCODE_M;
      case Key::N:
        return SDL_SCANCODE_N;
      case Key::O:
        return SDL_SCANCODE_O;
      case Key::P:
        return SDL_SCANCODE_P;
      case Key::Q:
        return SDL_SCANCODE_Q;
      case Key::R:
        return SDL_SCANCODE_R;
      case Key::S:
        return SDL_SCANCODE_S;
      case Key::T:
        return SDL_SCANCODE_T;
      case Key::U:
        return SDL_SCANCODE_U;
      case Key::V:
        return SDL_SCANCODE_V;
      case Key::W:
        return SDL_SCANCODE_W;
      case Key::X:
        return SDL_SCANCODE_X;
      case Key::Y:
        return SDL_SCANCODE_Y;
      case Key::Z:
        return SDL_SCANCODE_Z;
      case Key::Num0:
        return SDL_SCANCODE_0;
      case Key::Num1:
        return SDL_SCANCODE_1;
      case Key::Num2:
        return SDL_SCANCODE_2;
      case Key::Num3:
        return SDL_SCANCODE_3;
      case Key::Num4:
        return SDL_SCANCODE_4;
      case Key::Num5:
        return SDL_SCANCODE_5;
      case Key::Num6:
        return SDL_SCANCODE_6;
      case Key::Num7:
        return SDL_SCANCODE_7;
      case Key::Num8:
        return SDL_SCANCODE_8;
      case Key::Num9:
        return SDL_SCANCODE_9;
      case Key::Space:
        return SDL_SCANCODE_SPACE;
      case Key::Enter:
        return SDL_SCANCODE_RETURN;
      case Key::Escape:
        return SDL_SCANCODE_ESCAPE;
      case Key::Tab:
        return SDL_SCANCODE_TAB;
      case Key::Backspace:
        return SDL_SCANCODE_BACKSPACE;
      case Key::LeftShift:
        return SDL_SCANCODE_LSHIFT;
      case Key::RightShift:
        return SDL_SCANCODE_RSHIFT;
      case Key::LeftCtrl:
        return SDL_SCANCODE_LCTRL;
      case Key::RightCtrl:
        return SDL_SCANCODE_RCTRL;
      case Key::LeftAlt:
        return SDL_SCANCODE_LALT;
      case Key::RightAlt:
        return SDL_SCANCODE_RALT;
      case Key::Up:
        return SDL_SCANCODE_UP;
      case Key::Down:
        return SDL_SCANCODE_DOWN;
      case Key::Left:
        return SDL_SCANCODE_LEFT;
      case Key::Right:
        return SDL_SCANCODE_RIGHT;
      default:
        return SDL_SCANCODE_UNKNOWN;
    }
  }

  // Win32 virtual-key fallback, used only when no SDL_Window resource
  // exists (CHALK_PLATFORM=WIN32 with CHALK_INPUT=SDL3).
  [[nodiscard]] auto to_virtual_key(Key key) -> int
  {
    switch (key)
    {
      case Key::A:
        return 'A';
      case Key::B:
        return 'B';
      case Key::C:
        return 'C';
      case Key::D:
        return 'D';
      case Key::E:
        return 'E';
      case Key::F:
        return 'F';
      case Key::G:
        return 'G';
      case Key::H:
        return 'H';
      case Key::I:
        return 'I';
      case Key::J:
        return 'J';
      case Key::K:
        return 'K';
      case Key::L:
        return 'L';
      case Key::M:
        return 'M';
      case Key::N:
        return 'N';
      case Key::O:
        return 'O';
      case Key::P:
        return 'P';
      case Key::Q:
        return 'Q';
      case Key::R:
        return 'R';
      case Key::S:
        return 'S';
      case Key::T:
        return 'T';
      case Key::U:
        return 'U';
      case Key::V:
        return 'V';
      case Key::W:
        return 'W';
      case Key::X:
        return 'X';
      case Key::Y:
        return 'Y';
      case Key::Z:
        return 'Z';
      case Key::Num0:
        return '0';
      case Key::Num1:
        return '1';
      case Key::Num2:
        return '2';
      case Key::Num3:
        return '3';
      case Key::Num4:
        return '4';
      case Key::Num5:
        return '5';
      case Key::Num6:
        return '6';
      case Key::Num7:
        return '7';
      case Key::Num8:
        return '8';
      case Key::Num9:
        return '9';
      case Key::Space:
        return VK_SPACE;
      case Key::Enter:
        return VK_RETURN;
      case Key::Escape:
        return VK_ESCAPE;
      case Key::Tab:
        return VK_TAB;
      case Key::Backspace:
        return VK_BACK;
      case Key::LeftShift:
        return VK_LSHIFT;
      case Key::RightShift:
        return VK_RSHIFT;
      case Key::LeftCtrl:
        return VK_LCONTROL;
      case Key::RightCtrl:
        return VK_RCONTROL;
      case Key::LeftAlt:
        return VK_LMENU;
      case Key::RightAlt:
        return VK_RMENU;
      case Key::Up:
        return VK_UP;
      case Key::Down:
        return VK_DOWN;
      case Key::Left:
        return VK_LEFT;
      case Key::Right:
        return VK_RIGHT;
      default:
        return 0;
    }
  }

  auto poll_all_keys(InputState& state, auto&& is_key_down)
  {
    for (auto i = static_cast<std::size_t>(1); i < static_cast<std::size_t>(Key::Count); ++i)
    {
      auto key          = static_cast<Key>(i);
      state.key_down[i] = is_key_down(key);
    }
  }

} // namespace engine

export namespace engine
{

  class InputSDL3Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void InputSDL3Plugin::build(App& app)
  {
    app.insert_resource<InputState>({});

    // Decided once: platform.sdl3 (if active) has already inserted
    // SDL_Window* synchronously during its own build(), which runs before
    // this one (platform is added before input in plugin.default.ixx). No
    // per-frame branching needed.
    if (app.has_resource<SDL_Window*>())
    {
      app.report(Severity::Info, "input: sdl3 backend using SDL window state");

      app.add_system(
          Schedule::PreUpdate,
          [](App& app) -> void
          {
            auto& state               = app.require_resource<InputState>();
            state.key_down_previous   = state.key_down;
            state.mouse_down_previous = state.mouse_down;

            SDL_PumpEvents();

            poll_all_keys(
                state, [keys = SDL_GetKeyboardState(nullptr)](Key key) -> bool { return keys[to_sdl_scancode(key)]; }
            );

            float mouse_x = 0.0f;
            float mouse_y = 0.0f;
            auto buttons  = SDL_GetMouseState(&mouse_x, &mouse_y);

            state.mouse_down[static_cast<std::size_t>(MouseButton::Left)]   = (buttons & SDL_BUTTON_LMASK) != 0;
            state.mouse_down[static_cast<std::size_t>(MouseButton::Right)]  = (buttons & SDL_BUTTON_RMASK) != 0;
            state.mouse_down[static_cast<std::size_t>(MouseButton::Middle)] = (buttons & SDL_BUTTON_MMASK) != 0;

            state.mouse_delta_x = mouse_x - state.mouse_x;
            state.mouse_delta_y = mouse_y - state.mouse_y;
            state.mouse_x       = mouse_x;
            state.mouse_y       = mouse_y;
          }
      );

      return;
    }

    // Mandatory native fallback: no SDL window, so no SDL event pump
    // either. Same Win32-only restriction as renderer.sdl3.ixx's own
    // NativeWindowHandle fallback (X11/Wayland are not implemented yet).
    auto& native = app.require_resource<NativeWindowHandle>();
    if (native.kind != NativeWindowKind::Win32)
    {
      app.report(Severity::Fatal, "input: sdl3 backend has no native fallback for this platform");
    }

    app.report(Severity::Info, "input: sdl3 backend using native win32 state");
    HWND window = static_cast<HWND>(native.handle);

    app.add_system(
        Schedule::PreUpdate,
        [window](App& app) -> void
        {
          auto& state               = app.require_resource<InputState>();
          state.key_down_previous   = state.key_down;
          state.mouse_down_previous = state.mouse_down;

          poll_all_keys(state, [](Key key) -> bool { return (GetAsyncKeyState(to_virtual_key(key)) & 0x8000) != 0; });

          state.mouse_down[static_cast<std::size_t>(MouseButton::Left)]  = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
          state.mouse_down[static_cast<std::size_t>(MouseButton::Right)] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
          state.mouse_down[static_cast<std::size_t>(MouseButton::Middle)]
              = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

          POINT point{};
          GetCursorPos(&point);
          ScreenToClient(window, &point);

          auto mouse_x = static_cast<f32>(point.x);
          auto mouse_y = static_cast<f32>(point.y);

          state.mouse_delta_x = mouse_x - state.mouse_x;
          state.mouse_delta_y = mouse_y - state.mouse_y;
          state.mouse_x       = mouse_x;
          state.mouse_y       = mouse_y;
        }
    );
  }

} // namespace engine
