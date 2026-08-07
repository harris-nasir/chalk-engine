module;

#include <SDL3/SDL.h>

#include <optional>
#include <utility>
#include <vector>

export module engine.renderer.sdl3:utility;

import engine.core;
import engine.renderer.types;

namespace engine
{
  template <typename T>
  class HandleTable
  {
  public:
    auto insert(T value) -> u64
    {
      slots_.push_back(std::move(value));
      return slots_.size(); // 1-based; 0 is reserved for Invalid, and indices are never reused
    }

    [[nodiscard]] auto get(u64 handle) -> T*
    {
      if (handle == 0 || handle > slots_.size())
      {
        return nullptr;
      }
      auto& slot = slots_[handle - 1];
      return slot ? &*slot : nullptr;
    }

    auto destroy(u64 handle) -> bool
    {
      if (get(handle) == nullptr)
      {
        return false;
      }
      slots_[handle - 1].reset();
      return true;
    }

  private:
    std::vector<std::optional<T>> slots_;
  };

  struct TextureRecord
  {
    SDL_GPUTexture* handle;
    u32 width;
    u32 height;
    engine::PixelFormat format;
  };

  // Never a real HandleTable index (those start at 1 and grow one at a
  // time), so it can never collide with a created texture's handle.
  constexpr u64 SWAPCHAIN_TEXTURE_HANDLE = ~u64{0};

  [[nodiscard]] auto to_sdl_buffer_usage(engine::BufferUsage usage) -> SDL_GPUBufferUsageFlags
  {
    switch (usage)
    {
      case engine::BufferUsage::Vertex:
        return SDL_GPU_BUFFERUSAGE_VERTEX;
      case engine::BufferUsage::Index:
        return SDL_GPU_BUFFERUSAGE_INDEX;
      case engine::BufferUsage::Uniform:
        // SDL3 GPU has no persistent uniform-buffer resource: real uniform
        // data flows through SDL_PushGPU{Vertex,Fragment}UniformData, not a
        // bound buffer. GRAPHICS_STORAGE_READ is the closest legal usage so
        // a buffer created with this usage is still a valid SDL object,
        // usable as a shader storage buffer if a future plugin needs one.
        return SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    }
    return 0;
  }

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
} // namespace engine
