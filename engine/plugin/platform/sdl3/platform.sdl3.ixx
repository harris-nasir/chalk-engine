module;

#include <SDL3/SDL.h>
#include <utility>

export module engine.platform.sdl3;

import engine.core;
import engine.platform;

export namespace engine
{

  class PlatformSDL3Plugin
  {
  public:
    explicit PlatformSDL3Plugin(WindowDescription description = {}) : description_(std::move(description)) {}

    void build(App& app);

  private:
    WindowDescription description_;
  };

} // namespace engine

namespace engine
{

  void PlatformSDL3Plugin::build(App& app)
  {
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
      app.report(Severity::Fatal, "SDL_Init failed: {}", SDL_GetError());
    }

    SDL_WindowFlags flags = 0;
    if (description_.is_fullscreen)
    {
      flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (description_.is_hidden)
    {
      flags |= SDL_WINDOW_HIDDEN;
    }
    if (description_.is_borderless)
    {
      flags |= SDL_WINDOW_BORDERLESS;
    }
    if (description_.is_minimized)
    {
      flags |= SDL_WINDOW_MINIMIZED;
    }
    if (description_.is_maximized)
    {
      flags |= SDL_WINDOW_MAXIMIZED;
    }
    if (description_.is_resizeable)
    {
      flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(
        description_.title.c_str(), static_cast<int>(description_.width), static_cast<int>(description_.height), flags
    );
    if (window == nullptr)
    {
      app.report(Severity::Fatal, "SDL_CreateWindow failed: {}", SDL_GetError());
    }

    app.insert_resource<Window>({
        .title         = description_.title,
        .width         = description_.width,
        .height        = description_.height,
        .is_fullscreen = description_.is_fullscreen,
        .is_hidden     = description_.is_hidden,
        .is_borderless = description_.is_borderless,
        .is_minimized  = description_.is_minimized,
        .is_maximized  = description_.is_maximized,
        .is_resizeable = description_.is_resizeable,
    });

    // lets an SDL3-based renderer reuse this window directly instead of wrapping NativeWindowHandle.
    app.insert_resource<SDL_Window*>(window);

#ifdef SDL_PLATFORM_WIN32
    void* native_handle
        = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (native_handle == nullptr)
    {
      app.report(Severity::Fatal, "SDL_GetPointerProperty(SDL_PROP_WINDOW_WIN32_HWND_POINTER) failed");
    }
    app.insert_resource<NativeWindowHandle>({.kind = NativeWindowKind::Win32, .handle = native_handle});
#else
    app.report(Severity::Fatal, "native window handle extraction not implemented for this platform");
#endif

    app.report(
        Severity::Info, "opened window \"{}\" ({}x{})", description_.title, description_.width, description_.height
    );

    app.add_system(
        Schedule::PreUpdate,
        [](App& app) -> void
        {
          auto& window = app.require_resource<Window>();

          SDL_Event event;
          while (SDL_PollEvent(&event))
          {
            switch (event.type)
            {
              case SDL_EVENT_WINDOW_RESIZED:
              {
                window.width  = static_cast<u32>(event.window.data1);
                window.height = static_cast<u32>(event.window.data2);
                break;
              }
              case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
              case SDL_EVENT_QUIT:
              {
                window.should_close = true;
                break;
              }
              default:
              {
                break;
              }
            }
          }

          if (window.should_close)
          {
            app.exit();
          }
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [sdl_window = window](App& app) -> void
        {
          SDL_DestroyWindow(sdl_window);
          SDL_Quit();
          app.report(Severity::Info, "window closed");
        }
    );
  }

} // namespace engine
