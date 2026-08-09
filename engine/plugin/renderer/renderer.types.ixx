module;

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

export module engine.renderer.types;

import engine.core;

export namespace engine
{

  struct Color
  {
    f32 r, g, b, a;
  };

  enum class VertexBufferHandle : u64
  {
    Invalid = 0
  };

  enum class IndexBufferHandle : u64
  {
    Invalid = 0
  };

  enum class UniformBufferHandle : u64
  {
    Invalid = 0
  };

  using BufferHandle = std::variant<VertexBufferHandle, IndexBufferHandle, UniformBufferHandle>;

  enum class TextureHandle : u64
  {
    Invalid = 0
  };

  enum class ShaderHandle : u64
  {
    Invalid = 0
  };

  enum class PipelineHandle : u64
  {
    Invalid = 0
  };

  enum class FrameID : u64
  {
    Invalid = 0
  };

  enum class PassID : u64
  {
    Invalid = 0
  };

  enum class CopyPassID : u64
  {
    Invalid = 0
  };

  enum class BufferUsage : u8
  {
    Vertex,
    Index,
    Uniform,
  };

  struct BufferDescription
  {
    u64 size;
    BufferUsage usage;
  };

  enum class PixelFormat : u8
  {
    RGBA8,
    BGRA8,
    Depth24Stencil8,
  };

  enum class TextureUsage : u32
  {
    Sampled     = 1 << 0,
    ColorTarget = 1 << 1,
    DepthTarget = 1 << 2,
  };

  struct TextureDescription
  {
    u32 width, height;
    PixelFormat format;
    TextureUsage usage;
  };

  enum class VertexFormat : u8
  {
    F32,
    F32x2,
    F32x3,
    F32x4,
    U8x4Norm,
  };

  struct VertexAttribute
  {
    u32 location;
    u32 offset;
    VertexFormat format;
  };

  enum class Topology : u8
  {
    TriangleList,
    LineList,
    PointList,
  };

  struct BlendState
  {
    bool enabled;
  };

  struct DepthState
  {
    bool test_enabled;
    bool write_enabled;
  };

  enum class ShaderStage : u8
  {
    Vertex,
    Fragment,
  };

  enum class ShaderFormat : u8
  {
    Invalid,
    SPIRV,
    DXIL,
    MSL,
    HLSL,
  };

  struct ShaderResourceCounts
  {
    u32 samplers         = 0;
    u32 storage_textures = 0;
    u32 storage_buffers  = 0;
    u32 uniform_buffers  = 0;
  };

  struct ShaderDescription
  {
    std::span<const u8> code;
    ShaderStage stage;
    ShaderFormat format;
    const char* entry_point = "main";
    ShaderResourceCounts resources;
  };

  struct PipelineDescription
  {
    ShaderHandle vertex_shader;
    ShaderHandle fragment_shader;
    std::span<const VertexAttribute> vertex_layout;
    u32 vertex_stride;
    Topology topology;
    BlendState blend;
    DepthState depth;
  };

  enum class LoadOp : u8
  {
    Clear,
    Load,
    Ignore,
  };

  enum class StoreOp : u8
  {
    Store,
    Ignore,
  };

  struct ColorAttachment
  {
    TextureHandle target;
    LoadOp load;
    StoreOp store;
    Color clear;
  };

  struct DepthAttachment
  {
    TextureHandle target;
    LoadOp load;
    StoreOp store;
    f32 clear_depth;
  };

  struct RenderPassDescription
  {
    std::span<const ColorAttachment> color_attachments;
    Option<DepthAttachment> depth_attachment{}; // empty = no depth buffer bound
  };

  // Generic 1-based index table shared by every backend's resource records
  // (buffers/textures/shaders/pipelines): index 0 is reserved for Invalid,
  // and indices are never reused, so a stale handle reliably misses instead
  // of silently aliasing a newer resource.
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

  template <typename T>
  concept RendererBackend = requires(
      T& r, BufferDescription buffer_description, BufferHandle buffer, TextureDescription texture_description,
      ShaderDescription shader_description, PipelineDescription pipeline_description,
      RenderPassDescription pass_description, TextureHandle texture, ShaderHandle shader, PipelineHandle pipeline,
      FrameID frame, PassID pass, CopyPassID copy_pass, ShaderStage stage, u32 count, std::span<const std::byte> bytes
  ) {
    { r.create_buffer(buffer_description) } -> std::same_as<BufferHandle>;
    { r.destroy_buffer(buffer) } -> std::same_as<void>;
    { r.create_texture(texture_description) } -> std::same_as<TextureHandle>;
    { r.destroy_texture(texture) } -> std::same_as<void>;
    { r.create_shader(shader_description) } -> std::same_as<ShaderHandle>;
    { r.destroy_shader(shader) } -> std::same_as<void>;
    { r.create_pipeline(pipeline_description) } -> std::same_as<PipelineHandle>;
    { r.destroy_pipeline(pipeline) } -> std::same_as<void>;

    { r.begin_frame() } -> std::same_as<FrameID>;
    { r.swapchain_texture(frame) } -> std::same_as<TextureHandle>;
    { r.begin_copy_pass(frame) } -> std::same_as<CopyPassID>;
    { r.upload_buffer(copy_pass, buffer, bytes) } -> std::same_as<void>;
    { r.upload_texture(copy_pass, texture, bytes, count) } -> std::same_as<void>;
    { r.end_copy_pass(copy_pass) } -> std::same_as<void>;

    { r.begin_pass(frame, pass_description) } -> std::same_as<PassID>;
    { r.bind_pipeline(pass, pipeline) } -> std::same_as<void>;
    { r.bind_buffer(pass, buffer, count) } -> std::same_as<void>;
    { r.push_uniforms(pass, stage, count, bytes) } -> std::same_as<void>;
    { r.draw(pass, count, count, count) } -> std::same_as<void>;
    { r.draw_indexed(pass, count, count, count) } -> std::same_as<void>;
    { r.end_pass(pass) } -> std::same_as<void>;
    { r.submit(frame) } -> std::same_as<void>;
  };

} // namespace engine
