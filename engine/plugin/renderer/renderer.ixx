export module engine.renderer;

export import engine.renderer.types;

#if defined(CHALK_RENDERER_SDL3)

import engine.renderer.sdl3;

export namespace engine
{
  using Renderer = SDL3Renderer;
}

#elif defined(CHALK_RENDERER_DX11)

#error "CHALK_RENDERER=DX11 is not yet implemented against the engine.renderer contract (renderer.dx11.ixx still uses ad hoc D3D11 calls, not RendererBackend). See docs/superpowers/specs/2026-08-06-renderer-contract-design.md follow-ups."

#else

#error "CHALK_RENDERER must be defined to SDL3 or DX11 (set via the CHALK_RENDERER CMake option)"

#endif

static_assert(engine::RendererBackend<engine::Renderer>);
