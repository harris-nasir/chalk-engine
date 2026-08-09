module;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <utility>

export module engine.platform.win32;

import engine.core;
import engine.platform;

export namespace engine
{
  class PlatformWin32Plugin
  {
  public:
    explicit PlatformWin32Plugin(WindowDescription description = {}) : description_(std::move(description)) {}

    void build(App& app);

  private:
    WindowDescription description_;
  };
} // namespace engine

namespace
{
  auto CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) -> LRESULT
  {
    switch (message)
    {
      case WM_CLOSE:
      {
        PostQuitMessage(0);
        break;
      }

      case WM_SIZE:
      {
        // Set once, right after CreateWindowEx, via SetWindowLongPtr(GWLP_USERDATA, ...)
        // in PlatformWin32Plugin::build: this WNDPROC has no other way to reach
        // the Window resource, since Win32 dispatches to it directly.
        auto* window_resource = reinterpret_cast<engine::Window*>(GetWindowLongPtr(window, GWLP_USERDATA));
        if (window_resource != nullptr)
        {
          window_resource->width  = LOWORD(l_param);
          window_resource->height = HIWORD(l_param);
        }
        break;
      }

      default:
      {
        return DefWindowProc(window, message, w_param, l_param);
      }
    }

    return 0;
  }
} // namespace

namespace engine
{

  void PlatformWin32Plugin::build(App& app)
  {
    WNDCLASSEX window_class{
        .cbSize        = sizeof(WNDCLASSEX),
        .lpfnWndProc   = &window_procedure,
        .hIcon         = LoadIcon(nullptr, IDI_APPLICATION),
        .hCursor       = LoadCursor(nullptr, IDC_ARROW),
        .lpszClassName = "Win32Window",
    };
    auto window_class_id{RegisterClassEx(&window_class)};

    if (window_class_id == 0U)
    {
      app.report(Severity::Fatal, "registering window class failed: {}", GetLastError());
    }

    DWORD style = (description_.is_borderless || description_.is_fullscreen)
                      ? WS_POPUP
                      : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (description_.is_resizeable && !description_.is_fullscreen)
    {
      style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    int x      = CW_USEDEFAULT;
    int y      = CW_USEDEFAULT;
    u32 width  = description_.width;
    u32 height = description_.height;
    if (description_.is_fullscreen)
    {
      x      = 0;
      y      = 0;
      width  = static_cast<u32>(GetSystemMetrics(SM_CXSCREEN));
      height = static_cast<u32>(GetSystemMetrics(SM_CYSCREEN));
    }

    RECT rect{.left = 0, .top = 0, .right = static_cast<LONG>(width), .bottom = static_cast<LONG>(height)};
    AdjustWindowRect(&rect, style, 0);

    auto* window{CreateWindowEx(
        0,
        MAKEINTATOM(window_class_id),
        description_.title.c_str(),
        style,
        x,
        y,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    )};

    if (window == nullptr)
    {
      app.report(Severity::Fatal, "creating window failed: {}", GetLastError());
    }

    app.insert_resource<Window>({
        .title         = description_.title,
        .width         = width,
        .height        = height,
        .is_fullscreen = description_.is_fullscreen,
        .is_hidden     = description_.is_hidden,
        .is_borderless = description_.is_borderless,
        .is_minimized  = description_.is_minimized,
        .is_maximized  = description_.is_maximized,
        .is_resizeable = description_.is_resizeable,
    });

    app.insert_resource<NativeWindowHandle>({.kind = NativeWindowKind::Win32, .handle = window});

    // Lets window_procedure (a raw WNDPROC with no App& access) reach the
    // Window resource directly when it gets WM_SIZE; require_resource returns
    // a stable reference into App's resource map for as long as this Window
    // resource lives, which is the whole run (nothing re-inserts it later).
    SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app.require_resource<Window>()));

    app.report(Severity::Info, "opened window \"{}\" ({}x{})", description_.title, width, height);

    int show_command = SW_SHOW;
    if (description_.is_hidden)
    {
      show_command = SW_HIDE;
    }
    else if (description_.is_minimized)
    {
      show_command = SW_SHOWMINIMIZED;
    }
    else if (description_.is_maximized)
    {
      show_command = SW_SHOWMAXIMIZED;
    }
    ShowWindow(window, show_command);

    app.add_system(
        Schedule::PreUpdate,
        [](App& app) -> void
        {
          auto& window = app.require_resource<Window>();

          MSG message{};
          while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
          {
            TranslateMessage(&message);
            DispatchMessage(&message);
          }

          if (message.message == WM_QUIT)
          {
            window.should_close = true;
            app.exit();
          }
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [window](App& app) -> void
        {
          DestroyWindow(window);
          app.report(Severity::Info, "window closed");
        }
    );
  }
} // namespace engine
