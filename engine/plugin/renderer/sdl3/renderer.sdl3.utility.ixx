module;

#include <SDL3/SDL.h>

export module engine.renderer.sdl3:utility;

import engine.core;
import engine.renderer.types;

namespace engine
{
  struct BufferRecord
  {
    SDL_GPUBuffer* handle;
    u64 size;
  };

  struct TextureRecord
  {
    SDL_GPUTexture* handle;
    u32 width;
    u32 height;
    engine::PixelFormat format;
  };

  [[nodiscard]] auto to_sdl_pixel_format(engine::PixelFormat format) -> SDL_GPUTextureFormat
  {
    switch (format)
    {
      case engine::PixelFormat::RGBA8:
        return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      case engine::PixelFormat::BGRA8:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
      case engine::PixelFormat::Depth24Stencil8:
        return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
  }

  [[nodiscard]] auto bytes_per_pixel(engine::PixelFormat format) -> u32
  {
    switch (format)
    {
      case engine::PixelFormat::RGBA8:
      case engine::PixelFormat::BGRA8:
      case engine::PixelFormat::Depth24Stencil8:
        return 4;
    }
    return 4;
  }

  [[nodiscard]] auto to_sdl_texture_usage(engine::TextureUsage usage) -> SDL_GPUTextureUsageFlags
  {
    SDL_GPUTextureUsageFlags flags{};
    auto raw = static_cast<u32>(usage);
    if ((raw & static_cast<u32>(engine::TextureUsage::Sampled)) != 0U)
    {
      flags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }
    if ((raw & static_cast<u32>(engine::TextureUsage::ColorTarget)) != 0U)
    {
      flags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }
    if ((raw & static_cast<u32>(engine::TextureUsage::DepthTarget)) != 0U)
    {
      flags |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    }
    return flags;
  }

  [[nodiscard]] auto to_sdl_vertex_format(engine::VertexFormat format) -> SDL_GPUVertexElementFormat
  {
    switch (format)
    {
      case engine::VertexFormat::F32:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
      case engine::VertexFormat::F32x2:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
      case engine::VertexFormat::F32x3:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
      case engine::VertexFormat::F32x4:
        return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
      case engine::VertexFormat::U8x4Norm:
        return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    }
    return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
  }

  [[nodiscard]] auto to_sdl_primitive_type(engine::Topology topology) -> SDL_GPUPrimitiveType
  {
    switch (topology)
    {
      case engine::Topology::TriangleList:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
      case engine::Topology::LineList:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
      case engine::Topology::PointList:
        return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    }
    return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  }

  [[nodiscard]] auto to_sdl_load_op(engine::LoadOp op) -> SDL_GPULoadOp
  {
    switch (op)
    {
      case engine::LoadOp::Clear:
        return SDL_GPU_LOADOP_CLEAR;
      case engine::LoadOp::Load:
        return SDL_GPU_LOADOP_LOAD;
      case engine::LoadOp::Ignore:
        return SDL_GPU_LOADOP_DONT_CARE;
    }
    return SDL_GPU_LOADOP_DONT_CARE;
  }

  [[nodiscard]] auto to_sdl_store_op(engine::StoreOp op) -> SDL_GPUStoreOp
  {
    switch (op)
    {
      case engine::StoreOp::Store:
        return SDL_GPU_STOREOP_STORE;
      case engine::StoreOp::Ignore:
        return SDL_GPU_STOREOP_DONT_CARE;
    }
    return SDL_GPU_STOREOP_DONT_CARE;
  }

  [[nodiscard]] auto to_sdl_shader_stage(engine::ShaderStage stage) -> SDL_GPUShaderStage
  {
    switch (stage)
    {
      case engine::ShaderStage::Vertex:
        return SDL_GPU_SHADERSTAGE_VERTEX;
      case engine::ShaderStage::Fragment:
        return SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    return SDL_GPU_SHADERSTAGE_VERTEX;
  }

  [[nodiscard]] auto to_sdl_shader_format(engine::ShaderFormat format) -> SDL_GPUShaderFormat
  {
    switch (format)
    {
      case engine::ShaderFormat::Invalid:
        return SDL_GPU_SHADERFORMAT_INVALID;
      case engine::ShaderFormat::SPIRV:
        return SDL_GPU_SHADERFORMAT_SPIRV;
      case engine::ShaderFormat::DXIL:
        return SDL_GPU_SHADERFORMAT_DXIL;
      case engine::ShaderFormat::MSL:
        return SDL_GPU_SHADERFORMAT_MSL;
      case engine::ShaderFormat::HLSL:
        return SDL_GPU_SHADERFORMAT_INVALID;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
  }
} // namespace engine
