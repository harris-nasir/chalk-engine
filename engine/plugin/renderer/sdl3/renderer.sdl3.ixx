module;

#include <SDL3/SDL.h>

#include <array>

export module engine.renderer.sdl3;

import engine.core;
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
          SDL_Window* window = app.require_resource<SDL_Window*>();
      // TODO: add fallback if only native window is provided

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

          // Cap the backend to one frame in flight (the default is 2). SDL
          // 3.4.x signals a dedicated semaphore per swapchain image, so the
          // present-reuse race is gone; this cap just bounds queue depth and
          // latency for the uncapped engine loop. SDL's own GPU renderer uses
          // the same setting.
          if (!SDL_SetGPUAllowedFramesInFlight(device, 1))
          {
            app.report(Severity::Fatal, "SDL_SetGPUAllowedFramesInFlight failed: {}", SDL_GetError());
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

          FrameHandle frame    = renderer.begin_frame();
          TextureHandle target = renderer.swapchain_texture(frame);

          if (target != TextureHandle::Invalid)
          {
            std::array<ColorAttachment, 1> color_attachments{ColorAttachment{
                .target = target,
                .load   = LoadOp::Clear,
                .store  = StoreOp::Store,
                .clear  = Color{.r = 17.0f / 255.0f, .g = 17.0f / 255.0f, .b = 17.0f / 255.0f, .a = 1.0f},
            }};

            PassHandle pass = renderer.begin_pass(frame, RenderPassDescription{.color_attachments = color_attachments});
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
