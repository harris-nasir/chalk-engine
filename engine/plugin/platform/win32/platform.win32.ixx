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
      app.report(Severity::Fatal, "Failed to register window class: {}", GetLastError());
    }

    RECT rect{
        .left   = 0,
        .top    = 0,
        .right  = static_cast<LONG>(description_.width),
        .bottom = static_cast<LONG>(description_.height)
    };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0);

    auto* window{CreateWindowEx(
        0,
        MAKEINTATOM(window_class_id),
        description_.title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    )};

    if (window == nullptr)
    {
      app.report(Severity::Fatal, "Failed to create window: {}", GetLastError());
    }

    app.insert_resource<Window>({
        .title  = description_.title,
        .width  = description_.width,
        .height = description_.height,
    });

    app.insert_resource<NativeWindowHandle>({.kind = NativeWindowKind::Win32, .handle = window});

    app.report(
        Severity::Info, "opened window \"{}\" ({}x{})", description_.title, description_.width, description_.height
    );

    ShowWindow(window, SW_SHOW);

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
