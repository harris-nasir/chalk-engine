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

namespace engine
{
  void PlatformWin32Plugin::build(App& app)
  {
    auto& diagnostics = app.require_resource<Diagnostics>();

    WNDCLASSEX window_class{};
    window_class.cbSize        = sizeof(WNDCLASSEX);
    window_class.lpszClassName = "Win32Window";
    window_class.lpfnWndProc   = DefWindowProc;
    auto window_class_id{RegisterClassEx(&window_class)};

    RECT rect{
        .left   = 0,
        .top    = 0,
        .right  = static_cast<LONG>(description_.width),
        .bottom = static_cast<LONG>(description_.height)
    };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0);

    CreateWindowEx(
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
    );

    diagnostics.report(Severity::Debug, "Window Class ID: {}", window_class_id);
  }
} // namespace engine
