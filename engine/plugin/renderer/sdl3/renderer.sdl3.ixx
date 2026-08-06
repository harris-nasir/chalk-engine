module;

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

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
    i32 logical_width      = 1600;
    i32 logical_height     = 900;
    const bool* keys       = nullptr;
  };

  struct Texture
  {
    SDL_Texture* handle{};
    SDL_FRect source{};
    SDL_FRect destination{};
    SDL_ScaleMode mode{};
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

          SDL_SetRenderLogicalPresentation(
              state.renderer, state.logical_width, state.logical_height, SDL_LOGICAL_PRESENTATION_LETTERBOX
          );

          state.keys = SDL_GetKeyboardState(nullptr);

          app.insert_resource<State>(state);
          app.report(Severity::Info, "renderer created");
        }
    );

    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          auto& state = app.require_resource<State>();
          Texture idle_texture{
              .handle      = IMG_LoadTexture(state.renderer, "../assets/AXE1.png"),
              .source      = {.x = 0, .y = 0, .w = 32, .h = 32},
              .destination = {.x = 0, .y = 0, .w = 32, .h = 32},
              .mode        = SDL_SCALEMODE_NEAREST,
          }; // TODO: asset server?
          if (!idle_texture.handle)
          {
            app.report(Severity::Fatal, "IMG_LoadTexture failed: {}", SDL_GetError());
          }
          SDL_SetTextureScaleMode(idle_texture.handle, idle_texture.mode);

          app.insert_resource<Texture>(idle_texture);
          app.report(Severity::Info, "texture loaded");
        }
    );

    app.add_system(
        Schedule::Update,
        [](App& app)
        {
          auto& state        = app.require_resource<State>();
          auto& idle_texture = app.require_resource<Texture>();
          i32 floor          = state.logical_height;
          f32 x{10};
          f32 y{floor - idle_texture.source.h};

          idle_texture.destination.x = x;
          idle_texture.destination.y = y;
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& state{app.require_resource<State>()};

          SDL_SetRenderDrawColor(state.renderer, 17, 17, 17, 255);
          SDL_RenderClear(state.renderer);

          auto& idle_texture = app.require_resource<Texture>();
          SDL_RenderTexture(state.renderer, idle_texture.handle, &idle_texture.source, &idle_texture.destination);

          SDL_RenderPresent(state.renderer); // swap buffers & present
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

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          auto& idle_texture = app.require_resource<Texture>();
          SDL_DestroyTexture(idle_texture.handle);
          app.report(Severity::Info, "texture destroyed");
        }
    );
  }
} // namespace engine
