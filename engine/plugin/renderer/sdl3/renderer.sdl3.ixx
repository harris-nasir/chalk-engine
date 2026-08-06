module;

#include <SDL3/SDL.h>

export module engine.renderer.sdl3;

import engine.core;
import engine.platform;
import engine.renderer;

export namespace engine
{

  class RendererSDL3Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace
{
  struct State
  {
    SDL_Renderer* renderer = nullptr;
    SDL_Window* window     = nullptr;
  };
} // namespace

namespace engine
{
  void RendererSDL3Plugin::build(App& app)
  {
    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          State state{};

          if (auto existing = app.resource<SDL_Window*>(); existing)
          {
            state.window = *existing;
          }
          else
          {
            // Retrieve the platform's already-created native window as an
            // SDL_Window. This does not create a new native window, only an
            // SDL-side wrapper around one the platform already made.
            auto& native_window{app.require_resource<NativeWindowHandle>()};
            SDL_PropertiesID properties{SDL_CreateProperties()};

            switch (native_window.kind)
            {
              case NativeWindowKind::Win32:
              {
                SDL_SetPointerProperty(properties, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, native_window.handle);
                break;
              }

              default:
              {
                app.report(Severity::Fatal, "Unsupported native window kind");
              }
            }

            // TODO(hot-reload): this wrapper is only ever destroyed by process
            // exit. Fine for a single-shot run, but will leak per reload once
            // hot reload exists. Revisit once the threading model for reload
            // is decided (affects whether it's safe for this plugin to destroy
            // a window it wrapped itself).
            state.window = SDL_CreateWindowWithProperties(properties);
            SDL_DestroyProperties(properties);

            if (state.window == nullptr)
            {
              app.report(Severity::Fatal, "Failed to retrieve native window as SDL window: {}", SDL_GetError());
            }
          }

          state.renderer = SDL_CreateRenderer(state.window, nullptr);
          if (state.renderer == nullptr)
          {
            app.report(Severity::Fatal, "SDL_CreateRenderer failed: {}", SDL_GetError());
          }

          app.insert_resource<State>(state);
          app.report(Severity::Info, "renderer created");
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& state{app.require_resource<State>()};

          SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
          SDL_RenderClear(state.renderer);
          SDL_RenderPresent(state.renderer);
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          auto& state{app.require_resource<State>()};
          SDL_DestroyRenderer(state.renderer);
          app.report(Severity::Info, "renderer shut down");
        }
    );
  }
} // namespace engine
