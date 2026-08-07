module;

#include <concepts>
#include <span>

export module engine.renderer.types;

import engine.core;

export namespace engine
{

  struct Color
  {
    f32 r, g, b, a;
  };

  enum class BufferHandle : u64
  {
    Invalid = 0
  };
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

  // Bytecode content is backend-specific (SPIR-V for SDL3 GPU on this
  // project's Windows/Vulkan path, DXBC/DXIL/MSL for other backends),
  // supplied as a per-backend asset file. No shader cross-compiler exists
  // in this build; see the SDL3 backend plan's shader-format note.
  struct ShaderSource
  {
    std::span<const u8> bytecode;
    ShaderStage stage;
    const char* entry_point   = "main";
    u32 sampler_count         = 0;
    u32 storage_texture_count = 0;
    u32 storage_buffer_count  = 0;
    u32 uniform_buffer_count  = 0;
  };

  struct PipelineDescription
  {
    ShaderHandle vertex_shader, fragment_shader;
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

  template <typename T>
  concept RendererBackend = requires(
      T& r, BufferDescription buffer_description, TextureDescription texture_description, ShaderSource shader_source,
      PipelineDescription pipeline_description, RenderPassDescription pass_description, BufferHandle buffer,
      TextureHandle texture, ShaderHandle shader, PipelineHandle pipeline, FrameID frame, PassID pass,
      CopyPassID copy_pass, ShaderStage stage, u32 count, std::span<const u8> bytes
  ) {
    { r.create_buffer(buffer_description) } -> std::same_as<BufferHandle>;
    { r.destroy_buffer(buffer) } -> std::same_as<void>;
    { r.create_texture(texture_description) } -> std::same_as<TextureHandle>;
    { r.destroy_texture(texture) } -> std::same_as<void>;
    { r.create_shader(shader_source) } -> std::same_as<ShaderHandle>;
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
    { r.bind_vertex_buffer(pass, buffer, count) } -> std::same_as<void>;
    { r.bind_index_buffer(pass, buffer) } -> std::same_as<void>;
    { r.push_uniforms(pass, stage, count, bytes) } -> std::same_as<void>;
    { r.draw(pass, count, count, count) } -> std::same_as<void>;
    { r.draw_indexed(pass, count, count, count) } -> std::same_as<void>;
    { r.end_pass(pass) } -> std::same_as<void>;
    { r.submit(frame) } -> std::same_as<void>;
  };

} // namespace engine
