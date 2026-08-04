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
    auto& diagnostics = app.require_resource<Diagnostics>();

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
      diagnostics.report(Severity::Fatal, "SDL_Init failed: {}", SDL_GetError());
    }

    SDL_Window* window = SDL_CreateWindow(
        description_.title.c_str(), static_cast<int>(description_.width), static_cast<int>(description_.height), 0
    );
    if (window == nullptr)
    {
      diagnostics.report(Severity::Fatal, "SDL_CreateWindow failed: {}", SDL_GetError());
    }

    app.insert_resource<Window>({
        .title  = description_.title,
        .width  = description_.width,
        .height = description_.height,
    });

#ifdef SDL_PLATFORM_WIN32
    void* native_handle
        = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (native_handle == nullptr)
    {
      diagnostics.report(Severity::Fatal, "SDL_GetPointerProperty(SDL_PROP_WINDOW_WIN32_HWND_POINTER) failed");
    }
    app.insert_resource<NativeWindowHandle>({.kind = NativeWindowKind::Win32, .handle = native_handle});
#else
    diagnostics.report(Severity::Fatal, "native window handle extraction not implemented for this platform");
#endif

    diagnostics.report(
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
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
              window.should_close = true;
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
