module;

#include <SDL3/SDL.h>

#include <array>

export module engine.renderer.sdl3;

import engine.core;
import engine.platform;
import engine.renderer.types;

export import :renderer;

export namespace engine
{

  class RendererSDL3Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace engine
{

  void RendererSDL3Plugin::build(App& app)
  {
    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          SDL_Window* window = nullptr;
          if (app.has_resource<SDL_Window*>())
          {
            window = app.require_resource<SDL_Window*>();
          }
          else
          {
            auto& native = app.require_resource<NativeWindowHandle>();

            if (native.kind != NativeWindowKind::Win32)
            {
              app.report(Severity::Fatal, "SDL3 renderer: cannot wrap a non-Win32 native window");
            }

            SDL_PropertiesID props = SDL_CreateProperties();
            if (props == 0)
            {
              app.report(Severity::Fatal, "SDL_CreateProperties failed: {}", SDL_GetError());
            }

            SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, native.handle);
            window = SDL_CreateWindowWithProperties(props);
            SDL_DestroyProperties(props);

            if (window == nullptr)
            {
              app.report(Severity::Fatal, "SDL_CreateWindowWithProperties failed: {}", SDL_GetError());
            }
          }

#ifdef _DEBUG
          constexpr bool debug_mode = true;
#else
          constexpr bool debug_mode = false;
#endif

          SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug_mode, nullptr);
          if (device == nullptr)
          {
            app.report(Severity::Fatal, "SDL_CreateGPUDevice failed: {}", SDL_GetError());
          }

          if (!SDL_ClaimWindowForGPUDevice(device, window))
          {
            app.report(Severity::Fatal, "SDL_ClaimWindowForGPUDevice failed: {}", SDL_GetError());
          }

          app.insert_resource<SDL3Renderer>(SDL3Renderer{app, device, window});
          app.report(Severity::Info, "SDL3 GPU renderer ready");
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<SDL3Renderer>();

          FrameID frame        = renderer.begin_frame();
          TextureHandle target = renderer.swapchain_texture(frame);

          if (target != TextureHandle::Invalid)
          {
            std::array<ColorAttachment, 1> color_attachments{ColorAttachment{
                .target = target,
                .load   = LoadOp::Clear,  // clear the data already in the texture
                .store  = StoreOp::Store, // store the data i.e overwrite everything in the texture
                .clear  = Color{.r = 17.0F / 255.0F, .g = 17.0F / 255.0F, .b = 17.0F / 255.0F, .a = 1.0F},
            }};

            PassID pass = renderer.begin_pass(frame, RenderPassDescription{.color_attachments = color_attachments});
            renderer.end_pass(pass);
          }

          renderer.submit(frame);
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<SDL3Renderer>();
          SDL_ReleaseWindowFromGPUDevice(renderer.device(), renderer.window());
          SDL_DestroyGPUDevice(renderer.device());
          app.report(Severity::Info, "SDL3 GPU renderer shut down");
        }
    );
  }

} // namespace engine
