module;

#include <string>

export module engine.platform;

import engine.core;

export namespace engine
{

  struct WindowDescription
  {
    std::string title = "Untitled";
    u32 width         = 1280;
    u32 height        = 720;
  };

  enum class NativeWindowKind : u8
  {
    Win32,
    X11,
    Wayland,
  };

  struct NativeWindowHandle
  {
    NativeWindowKind kind;
    void* handle;
  };

  struct Window
  {
    std::string title;
    u32 width;
    u32 height;
    bool should_close = false;
  };

} // namespace engine
