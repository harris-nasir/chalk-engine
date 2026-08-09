module;

#include <format>
#include <string_view>

export module engine.renderer;

import engine.core;
export import engine.renderer.types;

#if defined(CHALK_RENDERER_SDL3)
import engine.renderer.sdl3;
#elif defined(CHALK_RENDERER_DX11)
import engine.renderer.dx11;
#else
#  error "unknown renderer backend; define CHALK_RENDERER_SDL3 or CHALK_RENDERER_DX11"
#endif

export namespace engine
{

#if defined(CHALK_RENDERER_SDL3)
  using Renderer = SDL3Renderer;
#elif defined(CHALK_RENDERER_DX11)
  using Renderer = DX11Renderer;
#endif

  // Backend-agnostic shader load: callers pass a stem (e.g. "position.vertex")
  [[nodiscard]] inline auto load_shader(std::string_view name) -> Shader
  {
#if defined(CHALK_RENDERER_SDL3)
    return Shader{
        .code   = read_file_as_bytes(std::format("shaders/{}.spv", name)),
        .format = ShaderFormat::SPIRV,
    };
#elif defined(CHALK_RENDERER_DX11)
    return Shader{
        .code   = read_file_as_bytes(std::format("assets/shaders/{}.hlsl", name)),
        .format = ShaderFormat::HLSL,
    };
#endif
  }

} // namespace engine

static_assert(engine::RendererBackend<engine::Renderer>);
