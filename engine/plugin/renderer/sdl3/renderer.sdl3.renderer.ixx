module;

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstring>
#include <limits>
#include <span>
#include <variant>
#include <vector>

export module engine.renderer.sdl3:renderer;

import engine.core;
import engine.renderer.types;
import :utility;

export namespace engine
{

  class SDL3Renderer
  {
  public:
    SDL3Renderer(App& app, SDL_GPUDevice* device, SDL_Window* window) : app_(app), device_(device), window_(window) {}

    auto create_buffer(BufferDescription description) -> BufferHandle;
    auto destroy_buffer(BufferHandle handle) -> void;
    auto create_texture(TextureDescription description) -> TextureHandle;
    auto destroy_texture(TextureHandle handle) -> void;
    auto create_shader(ShaderDescription description) -> ShaderHandle;
    auto destroy_shader(ShaderHandle handle) -> void;
    auto create_pipeline(PipelineDescription description) -> PipelineHandle;
    auto destroy_pipeline(PipelineHandle handle) -> void;

    auto begin_frame() -> FrameID;
    auto swapchain_texture(FrameID frame) -> TextureHandle;
    auto begin_copy_pass(FrameID frame) -> CopyPassID;
    auto upload_buffer(CopyPassID pass, BufferHandle buffer, std::span<const std::byte> bytes) -> void;
    auto upload_texture(CopyPassID pass, TextureHandle texture, std::span<const std::byte> bytes, u32 bytes_per_row)
        -> void;
    auto end_copy_pass(CopyPassID pass) -> void;

    auto begin_pass(FrameID frame, RenderPassDescription description) -> PassID;
    auto bind_pipeline(PassID pass, PipelineHandle pipeline) -> void;
    auto bind_buffer(PassID pass, BufferHandle buffer, u32 slot) -> void;
    auto push_uniforms(PassID pass, ShaderStage stage, u32 slot, std::span<const std::byte> bytes) -> void;
    auto draw(PassID pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void;
    auto draw_indexed(PassID pass, u32 index_count, u32 instance_count, u32 first_index) -> void;
    auto end_pass(PassID pass) -> void;
    auto submit(FrameID frame) -> void;

  private:
    [[nodiscard]] auto resolve_texture(TextureHandle handle) -> SDL_GPUTexture*;

    App& app_;
    SDL_GPUDevice* device_;
    SDL_Window* window_;

    HandleTable<BufferRecord> buffers_;
    HandleTable<TextureRecord> textures_;
    HandleTable<SDL_GPUShader*> shaders_;
    HandleTable<SDL_GPUGraphicsPipeline*> pipelines_;

    u64 frame_token_                            = 0;
    SDL_GPUCommandBuffer* frame_command_buffer_ = nullptr;
    SDL_GPUTexture* frame_swapchain_texture_    = nullptr;

    u64 copy_pass_token_        = 0;
    SDL_GPUCopyPass* copy_pass_ = nullptr;

    u64 pass_token_                 = 0;
    SDL_GPURenderPass* render_pass_ = nullptr;
  };

  static_assert(RendererBackend<SDL3Renderer>);

} // namespace engine

namespace engine
{
  namespace
  {
    [[nodiscard]] auto make_buffer_handle(BufferUsage type, u64 id) -> BufferHandle
    {
      switch (type)
      {
        case BufferUsage::Vertex:
          return static_cast<VertexBufferHandle>(id);
        case BufferUsage::Index:
          return static_cast<IndexBufferHandle>(id);
        case BufferUsage::Uniform:
          return static_cast<UniformBufferHandle>(id);
      }
      return BufferHandle{};
    }
  } // namespace

  auto SDL3Renderer::create_buffer(BufferDescription description) -> BufferHandle
  {
    if (description.size > std::numeric_limits<u32>::max())
    {
      app_.report(Severity::Error, "size {} exceeds SDL3 GPU's u32 buffer size limit", description.size);
      return make_buffer_handle(description.usage, 0);
    }

    SDL_GPUBufferUsageFlags usage{};
    switch (description.usage)
    {
      case BufferUsage::Vertex:
        usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        break;
      case BufferUsage::Index:
        usage = SDL_GPU_BUFFERUSAGE_INDEX;
        break;
      case BufferUsage::Uniform:
        // SDL3 GPU has no persistent uniform-buffer resource: real uniform data
        // flows through SDL_PushGPU{Vertex,Fragment}UniformData, not a bound
        // buffer. GRAPHICS_STORAGE_READ is the closest legal usage so the buffer
        // is still a valid SDL object, usable as a shader storage buffer later.
        usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        break;
      default:
        app_.report(Severity::Error, "unsupported buffer type {}", static_cast<u32>(description.usage));
        return make_buffer_handle(description.usage, 0);
    }

    SDL_GPUBufferCreateInfo info{
        .usage = usage,
        .size  = static_cast<u32>(description.size),
        .props = 0,
    };

    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &info);
    if (buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUBuffer failed: {}", SDL_GetError());
      return make_buffer_handle(description.usage, 0);
    }

    return make_buffer_handle(
        description.usage, buffers_.insert(BufferRecord{.handle = buffer, .size = description.size})
    );
  }

  auto SDL3Renderer::destroy_buffer(BufferHandle handle) -> void
  {
    const u64 id = std::visit([](auto buffer) -> auto { return static_cast<u64>(buffer); }, handle);
    auto* record = buffers_.get(id);
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }
    SDL_ReleaseGPUBuffer(device_, record->handle);
    buffers_.destroy(id);
  }

  auto SDL3Renderer::create_texture(TextureDescription description) -> TextureHandle
  {
    SDL_GPUTextureCreateInfo info{
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = to_sdl_pixel_format(description.format),
        .usage                = to_sdl_texture_usage(description.usage),
        .width                = description.width,
        .height               = description.height,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .sample_count         = SDL_GPU_SAMPLECOUNT_1,
        .props                = 0,
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
    if (texture == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUTexture failed: {}", SDL_GetError());
      return TextureHandle::Invalid;
    }
    return static_cast<TextureHandle>(textures_.insert(
        TextureRecord{
            .handle = texture, .width = description.width, .height = description.height, .format = description.format
        }
    ));
  }

  auto SDL3Renderer::destroy_texture(TextureHandle handle) -> void
  {
    auto* record = textures_.get(static_cast<u64>(handle));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed texture handle");
      return;
    }
    SDL_ReleaseGPUTexture(device_, record->handle);
    textures_.destroy(static_cast<u64>(handle));
  }

  auto SDL3Renderer::create_shader(ShaderDescription description) -> ShaderHandle
  {
    SDL_GPUShaderCreateInfo info{
        .code_size            = description.code.size(),
        .code                 = description.code.data(),
        .entrypoint           = description.entry_point,
        .format               = to_sdl_shader_format(description.format),
        .stage                = to_sdl_shader_stage(description.stage),
        .num_samplers         = description.resources.samplers,
        .num_storage_textures = description.resources.storage_textures,
        .num_storage_buffers  = description.resources.storage_buffers,
        .num_uniform_buffers  = description.resources.uniform_buffers,
        .props                = 0,
    };
    SDL_GPUShader* shader = SDL_CreateGPUShader(device_, &info);
    if (shader == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUShader failed: {}", SDL_GetError());
      return ShaderHandle::Invalid;
    }
    return static_cast<ShaderHandle>(shaders_.insert(shader));
  }

  auto SDL3Renderer::destroy_shader(ShaderHandle handle) -> void
  {
    auto* shader = shaders_.get(static_cast<u64>(handle));
    if (shader == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed shader handle");
      return;
    }
    SDL_ReleaseGPUShader(device_, *shader);
    shaders_.destroy(static_cast<u64>(handle));
  }

  auto SDL3Renderer::create_pipeline(PipelineDescription description) -> PipelineHandle
  {
    auto* vertex_shader   = shaders_.get(static_cast<u64>(description.vertex_shader));
    auto* fragment_shader = shaders_.get(static_cast<u64>(description.fragment_shader));
    if (vertex_shader == nullptr || fragment_shader == nullptr)
    {
      app_.report(Severity::Error, "invalid vertex or fragment shader handle");
      return PipelineHandle::Invalid;
    }

    std::vector<SDL_GPUVertexAttribute> attributes;
    attributes.reserve(description.vertex_layout.size());
    for (const auto& attribute : description.vertex_layout)
    {
      attributes.push_back(
          SDL_GPUVertexAttribute{
              .location    = attribute.location,
              .buffer_slot = 0,
              .format      = to_sdl_vertex_format(attribute.format),
              .offset      = attribute.offset,
          }
      );
    }

    SDL_GPUVertexBufferDescription vertex_buffer{
        .slot               = 0,
        .pitch              = description.vertex_stride,
        .input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0,
    };

    SDL_GPUColorTargetBlendState blend_state{
        .src_color_blendfactor = description.blend.enabled ? SDL_GPU_BLENDFACTOR_SRC_ALPHA : SDL_GPU_BLENDFACTOR_ONE,
        .dst_color_blendfactor
        = description.blend.enabled ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : SDL_GPU_BLENDFACTOR_ZERO,
        .color_blend_op        = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor
        = description.blend.enabled ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : SDL_GPU_BLENDFACTOR_ZERO,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .color_write_mask
        = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A,
        .enable_blend            = description.blend.enabled,
        .enable_color_write_mask = false,
        .padding1                = 0,
        .padding2                = 0,
    };

    // Pipelines target the real swapchain format for now; a pipeline
    // targeting an offscreen texture of a different format is future work
    // (PipelineDescription doesn't carry a target format yet, no caller
    // needs one until a render-to-texture feature plugin exists). Query it
    // rather than hardcoding: SDL3's Vulkan backend returns B8G8R8A8_UNORM
    // for SDR swapchains, not R8G8B8A8_UNORM.
    SDL_GPUColorTargetDescription color_target{
        .format      = SDL_GetGPUSwapchainTextureFormat(device_, window_),
        .blend_state = blend_state,
    };

    SDL_GPUGraphicsPipelineCreateInfo info{
        .vertex_shader   = *vertex_shader,
        .fragment_shader = *fragment_shader,
        .vertex_input_state
        = SDL_GPUVertexInputState{
            .vertex_buffer_descriptions = &vertex_buffer,
            .num_vertex_buffers         = 1,
            .vertex_attributes          = attributes.data(),
            .num_vertex_attributes      = static_cast<u32>(attributes.size()),
        },
        .primitive_type   = to_sdl_primitive_type(description.topology),
        .rasterizer_state = SDL_GPURasterizerState{
            .fill_mode                  = SDL_GPU_FILLMODE_FILL,
            .cull_mode                  = SDL_GPU_CULLMODE_NONE,
            .front_face                 = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = 0.0F,
            .depth_bias_clamp           = 0.0F,
            .depth_bias_slope_factor    = 0.0F,
            .enable_depth_bias          = false,
            .enable_depth_clip          = false,
            .padding1                   = 0,
            .padding2                   = 0,
        },
        .multisample_state = SDL_GPUMultisampleState{
            .sample_count             = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask              = 0,
            .enable_mask              = false,
            .enable_alpha_to_coverage = false,
            .padding2                 = 0,
            .padding3                 = 0,
        },
        .depth_stencil_state = SDL_GPUDepthStencilState{
            .compare_op = SDL_GPU_COMPAREOP_LESS,
            .back_stencil_state
            = SDL_GPUStencilOpState{
                .fail_op       = SDL_GPU_STENCILOP_KEEP,
                .pass_op       = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op    = SDL_GPU_COMPAREOP_NEVER,
            },
            .front_stencil_state
            = SDL_GPUStencilOpState{
                .fail_op       = SDL_GPU_STENCILOP_KEEP,
                .pass_op       = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op    = SDL_GPU_COMPAREOP_NEVER,
            },
            .compare_mask        = 0,
            .write_mask          = 0,
            .enable_depth_test   = description.depth.test_enabled,
            .enable_depth_write  = description.depth.write_enabled,
            .enable_stencil_test = false,
            .padding1            = 0,
            .padding2            = 0,
            .padding3            = 0,
        },
        .target_info = SDL_GPUGraphicsPipelineTargetInfo{
            .color_target_descriptions = &color_target,
            .num_color_targets         = 1,
            .depth_stencil_format
            = description.depth.test_enabled ? to_sdl_pixel_format(PixelFormat::Depth24Stencil8)
                                              : SDL_GPU_TEXTUREFORMAT_INVALID,
            .has_depth_stencil_target = description.depth.test_enabled,
            .padding1                 = 0,
            .padding2                 = 0,
            .padding3                 = 0,
        },
        .props = 0,
    };

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device_, &info);
    if (pipeline == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUGraphicsPipeline failed: {}", SDL_GetError());
      return PipelineHandle::Invalid;
    }
    return static_cast<PipelineHandle>(pipelines_.insert(pipeline));
  }

  auto SDL3Renderer::destroy_pipeline(PipelineHandle handle) -> void
  {
    auto* pipeline = pipelines_.get(static_cast<u64>(handle));
    if (pipeline == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed pipeline handle");
      return;
    }
    SDL_ReleaseGPUGraphicsPipeline(device_, *pipeline);
    pipelines_.destroy(static_cast<u64>(handle));
  }

  auto SDL3Renderer::resolve_texture(TextureHandle handle) -> SDL_GPUTexture*
  {
    if (static_cast<u64>(handle) == SWAPCHAIN_TEXTURE_HANDLE)
    {
      return frame_swapchain_texture_;
    }
    auto* record = textures_.get(static_cast<u64>(handle));
    return (record != nullptr) ? record->handle : nullptr;
  }

  auto SDL3Renderer::begin_frame() -> FrameID
  {
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    if (command_buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
      return FrameID::Invalid;
    }

    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window_, &swapchain, nullptr, nullptr))
    {
      app_.report(Severity::Error, "SDL_WaitAndAcquireGPUSwapchainTexture failed: {}", SDL_GetError());
      SDL_CancelGPUCommandBuffer(command_buffer);
      return FrameID::Invalid;
    }

    ++frame_token_;
    frame_command_buffer_    = command_buffer;
    frame_swapchain_texture_ = swapchain; // may be null, not an error but must be checked.
    return static_cast<FrameID>(frame_token_);
  }

  auto SDL3Renderer::swapchain_texture(FrameID frame) -> TextureHandle
  {
    if (frame == FrameID::Invalid || static_cast<u64>(frame) != frame_token_)
    {
      app_.report(Severity::Error, "frame id does not match the current frame");
      return TextureHandle::Invalid;
    }
    if (frame_swapchain_texture_ == nullptr)
    {
      return TextureHandle::Invalid;
    }
    return static_cast<TextureHandle>(SWAPCHAIN_TEXTURE_HANDLE);
  }

  auto SDL3Renderer::submit(FrameID frame) -> void
  {
    if (frame == FrameID::Invalid || static_cast<u64>(frame) != frame_token_ || frame_command_buffer_ == nullptr)
    {
      app_.report(Severity::Error, "frame id does not match the current frame");
      return;
    }
    if (render_pass_ != nullptr)
    {
      app_.report(Severity::Error, "called with a pass still open; call end_pass/end_copy_pass first");
      render_pass_ = nullptr;
      copy_pass_   = nullptr;
    }
    if (!SDL_SubmitGPUCommandBuffer(frame_command_buffer_))
    {
      app_.report(Severity::Error, "SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
    }
    frame_command_buffer_    = nullptr;
    frame_swapchain_texture_ = nullptr;
  }

  auto SDL3Renderer::begin_copy_pass(FrameID frame) -> CopyPassID
  {
    if (frame == FrameID::Invalid || static_cast<u64>(frame) != frame_token_ || frame_command_buffer_ == nullptr)
    {
      app_.report(Severity::Error, "frame id does not match the current frame");
      return CopyPassID::Invalid;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(frame_command_buffer_);
    if (pass == nullptr)
    {
      app_.report(Severity::Error, "SDL_BeginGPUCopyPass failed: {}", SDL_GetError());
      return CopyPassID::Invalid;
    }
    ++copy_pass_token_;
    copy_pass_ = pass;
    return static_cast<CopyPassID>(copy_pass_token_);
  }

  auto SDL3Renderer::end_copy_pass(CopyPassID pass) -> void
  {
    if (pass == CopyPassID::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "copy pass id does not match the current copy pass");
      return;
    }
    SDL_EndGPUCopyPass(copy_pass_);
    copy_pass_ = nullptr;
  }

  auto SDL3Renderer::upload_buffer(CopyPassID pass, BufferHandle buffer, std::span<const std::byte> bytes) -> void
  {
    if (pass == CopyPassID::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "copy pass id does not match the current copy pass");
      return;
    }
    const u64 id = std::visit([](auto buffer) -> auto { return static_cast<u64>(buffer); }, buffer);
    auto* record = buffers_.get(id);
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }
    if (bytes.size() > record->size)
    {
      app_.report(
          Severity::Error, "data size {} exceeds buffer size {} (id: {})", bytes.size(), record->size, id
      );
      return;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = static_cast<u32>(bytes.size()),
        .props = 0,
    };
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
    if (transfer_buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError());
      return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_buffer, false);
    if (mapped == nullptr)
    {
      app_.report(Severity::Error, "SDL_MapGPUTransferBuffer failed: {}", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
      return;
    }
    std::memcpy(mapped, bytes.data(), bytes.size());

    SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
    SDL_GPUBufferRegion destination_region{
        .buffer = record->handle, .offset = 0, .size = static_cast<u32>(bytes.size())
    };
    SDL_UploadToGPUBuffer(copy_pass_, &source, &destination_region, false);

    SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
  }

  auto SDL3Renderer::upload_texture(
      CopyPassID pass, TextureHandle texture, std::span<const std::byte> bytes, u32 bytes_per_row
  ) -> void
  {
    if (pass == CopyPassID::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "copy pass id does not match the current copy pass");
      return;
    }
    if (static_cast<u64>(texture) == SWAPCHAIN_TEXTURE_HANDLE)
    {
      // Uploading raw bytes directly into the backbuffer isn't a supported
      // path; render into a regular texture and sample/blit it in a pass.
      app_.report(Severity::Error, "cannot upload directly into the swapchain texture");
      return;
    }
    auto* record = textures_.get(static_cast<u64>(texture));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed texture handle");
      return;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = static_cast<u32>(bytes.size()),
        .props = 0,
    };
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
    if (transfer_buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError());
      return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_buffer, false);
    if (mapped == nullptr)
    {
      app_.report(Severity::Error, "SDL_MapGPUTransferBuffer failed: {}", SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
      return;
    }
    std::memcpy(mapped, bytes.data(), bytes.size());
    SDL_UnmapGPUTransferBuffer(device_, transfer_buffer);

    SDL_GPUTextureTransferInfo source{
        .transfer_buffer = transfer_buffer,
        .offset          = 0,
        // pixels_per_row is a pixel count; bytes_per_row is a byte pitch.
        .pixels_per_row = bytes_per_row / bytes_per_pixel(record->format),
        .rows_per_layer = record->height,
    };
    // Region width/height come from the texture's own stored dimensions
    // (set at create_texture time), not from bytes_per_row: bytes-per-row
    // is a byte pitch, and deriving pixel width from it would be wrong for
    // any format wider than 1 byte/pixel.
    SDL_GPUTextureRegion destination_region{
        .texture   = record->handle,
        .mip_level = 0,
        .layer     = 0,
        .x         = 0,
        .y         = 0,
        .z         = 0,
        .w         = record->width,
        .h         = record->height,
        .d         = 1,
    };
    SDL_UploadToGPUTexture(copy_pass_, &source, &destination_region, false);

    SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
  }

  auto SDL3Renderer::begin_pass(FrameID frame, RenderPassDescription description) -> PassID
  {
    if (frame == FrameID::Invalid || static_cast<u64>(frame) != frame_token_)
    {
      app_.report(Severity::Error, "frame id does not match the current frame");
      return PassID::Invalid;
    }

    std::vector<SDL_GPUColorTargetInfo> color_targets;
    color_targets.reserve(description.color_attachments.size());
    for (const auto& attachment : description.color_attachments)
    {
      SDL_GPUTexture* texture = resolve_texture(attachment.target);
      if (texture == nullptr)
      {
        app_.report(Severity::Error, "color attachment has an invalid texture handle");
        return PassID::Invalid;
      }

      color_targets.push_back(SDL_GPUColorTargetInfo{
          .texture               = texture,
          .mip_level             = 0,
          .layer_or_depth_plane  = 0,
          .clear_color           = SDL_FColor{
              .r = attachment.clear.r, .g = attachment.clear.g, .b = attachment.clear.b, .a = attachment.clear.a
          },
          .load_op               = to_sdl_load_op(attachment.load),
          .store_op              = to_sdl_store_op(attachment.store),
          .resolve_texture       = nullptr,
          .resolve_mip_level     = 0,
          .resolve_layer         = 0,
          .cycle                 = false,
          .cycle_resolve_texture = false,
          .padding1              = 0,
          .padding2              = 0,
      });
    }

    SDL_GPUDepthStencilTargetInfo depth_target{};
    const SDL_GPUDepthStencilTargetInfo* depth_target_ptr = nullptr;
    if (description.depth_attachment)
    {
      SDL_GPUTexture* texture = resolve_texture(description.depth_attachment->target);
      if (texture == nullptr)
      {
        app_.report(Severity::Error, "depth attachment has an invalid texture handle");
        return PassID::Invalid;
      }
      depth_target = SDL_GPUDepthStencilTargetInfo{
          .texture          = texture,
          .clear_depth      = description.depth_attachment->clear_depth,
          .load_op          = to_sdl_load_op(description.depth_attachment->load),
          .store_op         = to_sdl_store_op(description.depth_attachment->store),
          .stencil_load_op  = SDL_GPU_LOADOP_LOAD,
          .stencil_store_op = SDL_GPU_STOREOP_STORE,
          .cycle            = false,
          .clear_stencil    = 0,
          .mip_level        = 0,
          .layer            = 0,
      };
      depth_target_ptr = &depth_target;
    }

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
        frame_command_buffer_, color_targets.data(), static_cast<u32>(color_targets.size()), depth_target_ptr
    );
    if (pass == nullptr)
    {
      app_.report(Severity::Error, "SDL_BeginGPURenderPass failed: {}", SDL_GetError());
      return PassID::Invalid;
    }

    ++pass_token_;
    render_pass_ = pass;
    return static_cast<PassID>(pass_token_);
  }

  auto SDL3Renderer::bind_pipeline(PassID pass, PipelineHandle pipeline) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    auto* handle = pipelines_.get(static_cast<u64>(pipeline));
    if (handle == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed pipeline handle");
      return;
    }
    SDL_BindGPUGraphicsPipeline(render_pass_, *handle);
  }

  auto SDL3Renderer::bind_buffer(PassID pass, BufferHandle buffer, u32 slot) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    const u64 id = std::visit([](auto b) -> auto { return static_cast<u64>(b); }, buffer);
    auto* record = buffers_.get(id);
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }

    SDL_GPUBufferBinding binding{.buffer = record->handle, .offset = 0};
    if (std::holds_alternative<VertexBufferHandle>(buffer))
    {
      SDL_BindGPUVertexBuffers(render_pass_, slot, &binding, 1);
    }
    else if (std::holds_alternative<IndexBufferHandle>(buffer))
    {
      SDL_BindGPUIndexBuffer(render_pass_, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    }
    else
    {
      app_.report(Severity::Error, "uniform buffers cannot be bound to a render pass");
    }
  }

  auto SDL3Renderer::push_uniforms(PassID pass, ShaderStage stage, u32 slot, std::span<const std::byte> bytes) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    switch (stage)
    {
      case ShaderStage::Vertex:
        SDL_PushGPUVertexUniformData(frame_command_buffer_, slot, bytes.data(), static_cast<u32>(bytes.size()));
        break;
      case ShaderStage::Fragment:
        SDL_PushGPUFragmentUniformData(frame_command_buffer_, slot, bytes.data(), static_cast<u32>(bytes.size()));
        break;
    }
  }

  auto SDL3Renderer::draw(PassID pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    SDL_DrawGPUPrimitives(render_pass_, vertex_count, instance_count, first_vertex, 0);
  }

  auto SDL3Renderer::draw_indexed(PassID pass, u32 index_count, u32 instance_count, u32 first_index) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    SDL_DrawGPUIndexedPrimitives(render_pass_, index_count, instance_count, first_index, 0, 0);
  }

  auto SDL3Renderer::end_pass(PassID pass) -> void
  {
    if (pass == PassID::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "pass id does not match the current pass");
      return;
    }
    SDL_EndGPURenderPass(render_pass_);
    render_pass_ = nullptr;
  }
} // namespace engine
