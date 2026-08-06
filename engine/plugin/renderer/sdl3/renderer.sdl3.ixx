module;

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <cstring>
#include <optional>
#include <span>
#include <vector>

export module engine.renderer.sdl3;

import engine.core;
import engine.platform;
import engine.renderer.types;

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
    if (raw & static_cast<u32>(engine::TextureUsage::Sampled))
    {
      flags |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    }
    if (raw & static_cast<u32>(engine::TextureUsage::ColorTarget))
    {
      flags |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    }
    if (raw & static_cast<u32>(engine::TextureUsage::DepthTarget))
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
      case engine::LoadOp::DontCare:
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
      case engine::StoreOp::DontCare:
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
} // namespace

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
    auto create_shader(ShaderSource source) -> ShaderHandle;
    auto destroy_shader(ShaderHandle handle) -> void;
    auto create_pipeline(PipelineDescription description) -> PipelineHandle;
    auto destroy_pipeline(PipelineHandle handle) -> void;

    [[nodiscard]] auto device() const -> SDL_GPUDevice* { return device_; }
    [[nodiscard]] auto window() const -> SDL_Window* { return window_; }

    auto begin_frame() -> FrameHandle;
    auto swapchain_texture(FrameHandle frame) -> TextureHandle;
    auto begin_copy_pass(FrameHandle frame) -> CopyPassHandle;
    auto upload_buffer(CopyPassHandle pass, BufferHandle buffer, std::span<const u8> bytes) -> void;
    auto upload_texture(CopyPassHandle pass, TextureHandle texture, std::span<const u8> bytes, u32 bytes_per_row)
        -> void;
    auto end_copy_pass(CopyPassHandle pass) -> void;

    auto begin_pass(FrameHandle frame, RenderPassDescription description) -> PassHandle;
    auto bind_pipeline(PassHandle pass, PipelineHandle pipeline) -> void;
    auto bind_vertex_buffer(PassHandle pass, BufferHandle buffer, u32 slot) -> void;
    auto bind_index_buffer(PassHandle pass, BufferHandle buffer) -> void;
    auto push_uniforms(PassHandle pass, ShaderStage stage, u32 slot, std::span<const u8> bytes) -> void;
    auto draw(PassHandle pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void;
    auto draw_indexed(PassHandle pass, u32 index_count, u32 instance_count, u32 first_index) -> void;
    auto end_pass(PassHandle pass) -> void;
    auto submit(FrameHandle frame) -> void;

  private:
    [[nodiscard]] auto resolve_texture(TextureHandle handle) -> SDL_GPUTexture*;

    App& app_;
    SDL_GPUDevice* device_;
    SDL_Window* window_;

    HandleTable<SDL_GPUBuffer*> buffers_;
    HandleTable<TextureRecord> textures_;
    HandleTable<SDL_GPUShader*> shaders_;
    HandleTable<SDL_GPUGraphicsPipeline*> pipelines_;

    u64 frame_token_                            = 0;
    SDL_GPUCommandBuffer* frame_command_buffer_ = nullptr;
    SDL_GPUTexture* frame_swapchain_texture_    = nullptr;

    u64 copy_pass_token_        = 0;
    SDL_GPUCopyPass* copy_pass_ = nullptr;

    u64 pass_token_                = 0;
    SDL_GPURenderPass* render_pass_ = nullptr;
  };

  static_assert(RendererBackend<SDL3Renderer>);

} // namespace engine

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

  auto SDL3Renderer::create_buffer(BufferDescription description) -> BufferHandle
  {
    SDL_GPUBufferCreateInfo info{
        .usage = to_sdl_buffer_usage(description.usage),
        .size  = static_cast<u32>(description.size),
    };
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &info);
    if (buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUBuffer failed: {}", SDL_GetError());
      return BufferHandle::Invalid;
    }
    return static_cast<BufferHandle>(buffers_.insert(buffer));
  }

  auto SDL3Renderer::destroy_buffer(BufferHandle handle) -> void
  {
    auto* buffer = buffers_.get(static_cast<u64>(handle));
    if (buffer == nullptr)
    {
      app_.report(Severity::Error, "destroy_buffer: invalid or already-destroyed handle");
      return;
    }
    SDL_ReleaseGPUBuffer(device_, *buffer);
    buffers_.destroy(static_cast<u64>(handle));
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
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
    if (texture == nullptr)
    {
      app_.report(Severity::Error, "SDL_CreateGPUTexture failed: {}", SDL_GetError());
      return TextureHandle::Invalid;
    }
    return static_cast<TextureHandle>(textures_.insert(TextureRecord{
        .handle = texture, .width = description.width, .height = description.height, .format = description.format
    }));
  }

  auto SDL3Renderer::destroy_texture(TextureHandle handle) -> void
  {
    auto* record = textures_.get(static_cast<u64>(handle));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "destroy_texture: invalid or already-destroyed handle");
      return;
    }
    SDL_ReleaseGPUTexture(device_, record->handle);
    textures_.destroy(static_cast<u64>(handle));
  }

  auto SDL3Renderer::create_shader(ShaderSource source) -> ShaderHandle
  {
    SDL_GPUShaderCreateInfo info{
        .code_size            = source.bytecode.size(),
        .code                 = source.bytecode.data(),
        .entrypoint           = source.entry_point,
        .format               = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage                = to_sdl_shader_stage(source.stage),
        .num_samplers         = source.sampler_count,
        .num_storage_textures = source.storage_texture_count,
        .num_storage_buffers  = source.storage_buffer_count,
        .num_uniform_buffers  = source.uniform_buffer_count,
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
      app_.report(Severity::Error, "destroy_shader: invalid or already-destroyed handle");
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
      app_.report(Severity::Error, "create_pipeline: invalid vertex or fragment shader handle");
      return PipelineHandle::Invalid;
    }

    std::vector<SDL_GPUVertexAttribute> attributes;
    attributes.reserve(description.vertex_layout.size());
    for (const auto& attribute : description.vertex_layout)
    {
      attributes.push_back(SDL_GPUVertexAttribute{
          .location    = attribute.location,
          .buffer_slot = 0,
          .format      = to_sdl_vertex_format(attribute.format),
          .offset      = attribute.offset,
      });
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
        .enable_blend   = description.blend.enabled,
    };

    // Pipelines target the swapchain's RGBA8 format for now; a pipeline
    // targeting an offscreen texture of a different format is future work
    // (PipelineDescription doesn't carry a target format yet, no caller
    // needs one until a render-to-texture feature plugin exists).
    SDL_GPUColorTargetDescription color_target{
        .format      = to_sdl_pixel_format(PixelFormat::RGBA8),
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
            .fill_mode  = SDL_GPU_FILLMODE_FILL,
            .cull_mode  = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .multisample_state   = SDL_GPUMultisampleState{.sample_count = SDL_GPU_SAMPLECOUNT_1},
        .depth_stencil_state = SDL_GPUDepthStencilState{
            .compare_op         = SDL_GPU_COMPAREOP_LESS,
            .enable_depth_test  = description.depth.test_enabled,
            .enable_depth_write = description.depth.write_enabled,
        },
        .target_info = SDL_GPUGraphicsPipelineTargetInfo{
            .color_target_descriptions = &color_target,
            .num_color_targets         = 1,
            .depth_stencil_format
            = description.depth.test_enabled ? to_sdl_pixel_format(PixelFormat::Depth24Stencil8)
                                              : SDL_GPU_TEXTUREFORMAT_INVALID,
            .has_depth_stencil_target = description.depth.test_enabled,
        },
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
      app_.report(Severity::Error, "destroy_pipeline: invalid or already-destroyed handle");
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
    return record ? record->handle : nullptr;
  }

  auto SDL3Renderer::begin_frame() -> FrameHandle
  {
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    if (command_buffer == nullptr)
    {
      app_.report(Severity::Error, "SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
      return FrameHandle::Invalid;
    }

    SDL_GPUTexture* swapchain = nullptr;
    if (!SDL_AcquireGPUSwapchainTexture(command_buffer, window_, &swapchain, nullptr, nullptr))
    {
      app_.report(Severity::Error, "SDL_AcquireGPUSwapchainTexture failed: {}", SDL_GetError());
      SDL_CancelGPUCommandBuffer(command_buffer);
      return FrameHandle::Invalid;
    }

    ++frame_token_;
    frame_command_buffer_    = command_buffer;
    frame_swapchain_texture_ = swapchain; // may be null if the window is minimized this frame; callers check swapchain_texture()'s result
    return static_cast<FrameHandle>(frame_token_);
  }

  auto SDL3Renderer::swapchain_texture(FrameHandle frame) -> TextureHandle
  {
    if (frame == FrameHandle::Invalid || static_cast<u64>(frame) != frame_token_)
    {
      app_.report(Severity::Error, "swapchain_texture: frame handle does not match the current frame");
      return TextureHandle::Invalid;
    }
    if (frame_swapchain_texture_ == nullptr)
    {
      return TextureHandle::Invalid;
    }
    return static_cast<TextureHandle>(SWAPCHAIN_TEXTURE_HANDLE);
  }

  auto SDL3Renderer::submit(FrameHandle frame) -> void
  {
    if (frame == FrameHandle::Invalid || static_cast<u64>(frame) != frame_token_ || frame_command_buffer_ == nullptr)
    {
      app_.report(Severity::Error, "submit: frame handle does not match the current frame");
      return;
    }
    if (render_pass_ != nullptr || copy_pass_ != nullptr)
    {
      app_.report(Severity::Error, "submit: called with a pass still open; call end_pass/end_copy_pass first");
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

  auto SDL3Renderer::begin_copy_pass(FrameHandle frame) -> CopyPassHandle
  {
    if (frame == FrameHandle::Invalid || static_cast<u64>(frame) != frame_token_ || frame_command_buffer_ == nullptr)
    {
      app_.report(Severity::Error, "begin_copy_pass: frame handle does not match the current frame");
      return CopyPassHandle::Invalid;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(frame_command_buffer_);
    if (pass == nullptr)
    {
      app_.report(Severity::Error, "SDL_BeginGPUCopyPass failed: {}", SDL_GetError());
      return CopyPassHandle::Invalid;
    }
    ++copy_pass_token_;
    copy_pass_ = pass;
    return static_cast<CopyPassHandle>(copy_pass_token_);
  }

  auto SDL3Renderer::end_copy_pass(CopyPassHandle pass) -> void
  {
    if (pass == CopyPassHandle::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "end_copy_pass: copy pass handle does not match the current copy pass");
      return;
    }
    SDL_EndGPUCopyPass(copy_pass_);
    copy_pass_ = nullptr;
  }

  auto SDL3Renderer::upload_buffer(CopyPassHandle pass, BufferHandle buffer, std::span<const u8> bytes) -> void
  {
    if (pass == CopyPassHandle::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "upload_buffer: copy pass handle does not match the current copy pass");
      return;
    }
    auto* destination = buffers_.get(static_cast<u64>(buffer));
    if (destination == nullptr)
    {
      app_.report(Severity::Error, "upload_buffer: invalid or already-destroyed buffer handle");
      return;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = static_cast<u32>(bytes.size()),
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

    SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
    SDL_GPUBufferRegion destination_region{.buffer = *destination, .offset = 0, .size = static_cast<u32>(bytes.size())};
    SDL_UploadToGPUBuffer(copy_pass_, &source, &destination_region, false);

    SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
  }

  auto SDL3Renderer::upload_texture(CopyPassHandle pass, TextureHandle texture, std::span<const u8> bytes, u32 bytes_per_row)
      -> void
  {
    if (pass == CopyPassHandle::Invalid || static_cast<u64>(pass) != copy_pass_token_ || copy_pass_ == nullptr)
    {
      app_.report(Severity::Error, "upload_texture: copy pass handle does not match the current copy pass");
      return;
    }
    if (static_cast<u64>(texture) == SWAPCHAIN_TEXTURE_HANDLE)
    {
      // Uploading raw bytes directly into the backbuffer isn't a supported
      // path; render into a regular texture and sample/blit it in a pass.
      app_.report(Severity::Error, "upload_texture: cannot upload directly into the swapchain texture");
      return;
    }
    auto* record = textures_.get(static_cast<u64>(texture));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "upload_texture: invalid or already-destroyed texture handle");
      return;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = static_cast<u32>(bytes.size()),
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

  auto SDL3Renderer::begin_pass(FrameHandle frame, RenderPassDescription description) -> PassHandle
  {
    if (frame == FrameHandle::Invalid || static_cast<u64>(frame) != frame_token_)
    {
      app_.report(Severity::Error, "begin_pass: frame handle does not match the current frame");
      return PassHandle::Invalid;
    }

    std::vector<SDL_GPUColorTargetInfo> color_targets;
    color_targets.reserve(description.color_attachments.size());
    for (const auto& attachment : description.color_attachments)
    {
      SDL_GPUTexture* texture = resolve_texture(attachment.target);
      if (texture == nullptr)
      {
        app_.report(Severity::Error, "begin_pass: color attachment has an invalid texture handle");
        return PassHandle::Invalid;
      }
      color_targets.push_back(SDL_GPUColorTargetInfo{
          .texture     = texture,
          .clear_color = SDL_FColor{
              .r = attachment.clear.r, .g = attachment.clear.g, .b = attachment.clear.b, .a = attachment.clear.a
          },
          .load_op  = to_sdl_load_op(attachment.load),
          .store_op = to_sdl_store_op(attachment.store),
      });
    }

    SDL_GPUDepthStencilTargetInfo depth_target{};
    const SDL_GPUDepthStencilTargetInfo* depth_target_ptr = nullptr;
    if (description.depth_attachment)
    {
      SDL_GPUTexture* texture = resolve_texture(description.depth_attachment->target);
      if (texture == nullptr)
      {
        app_.report(Severity::Error, "begin_pass: depth attachment has an invalid texture handle");
        return PassHandle::Invalid;
      }
      depth_target = SDL_GPUDepthStencilTargetInfo{
          .texture     = texture,
          .clear_depth = description.depth_attachment->clear_depth,
          .load_op     = to_sdl_load_op(description.depth_attachment->load),
          .store_op    = to_sdl_store_op(description.depth_attachment->store),
      };
      depth_target_ptr = &depth_target;
    }

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
        frame_command_buffer_, color_targets.data(), static_cast<u32>(color_targets.size()), depth_target_ptr
    );
    if (pass == nullptr)
    {
      app_.report(Severity::Error, "SDL_BeginGPURenderPass failed: {}", SDL_GetError());
      return PassHandle::Invalid;
    }

    ++pass_token_;
    render_pass_ = pass;
    return static_cast<PassHandle>(pass_token_);
  }

  auto SDL3Renderer::bind_pipeline(PassHandle pass, PipelineHandle pipeline) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "bind_pipeline: pass handle does not match the current pass");
      return;
    }
    auto* handle = pipelines_.get(static_cast<u64>(pipeline));
    if (handle == nullptr)
    {
      app_.report(Severity::Error, "bind_pipeline: invalid or already-destroyed pipeline handle");
      return;
    }
    SDL_BindGPUGraphicsPipeline(render_pass_, *handle);
  }

  auto SDL3Renderer::bind_vertex_buffer(PassHandle pass, BufferHandle buffer, u32 slot) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "bind_vertex_buffer: pass handle does not match the current pass");
      return;
    }
    auto* handle = buffers_.get(static_cast<u64>(buffer));
    if (handle == nullptr)
    {
      app_.report(Severity::Error, "bind_vertex_buffer: invalid or already-destroyed buffer handle");
      return;
    }
    SDL_GPUBufferBinding binding{.buffer = *handle, .offset = 0};
    SDL_BindGPUVertexBuffers(render_pass_, slot, &binding, 1);
  }

  auto SDL3Renderer::bind_index_buffer(PassHandle pass, BufferHandle buffer) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "bind_index_buffer: pass handle does not match the current pass");
      return;
    }
    auto* handle = buffers_.get(static_cast<u64>(buffer));
    if (handle == nullptr)
    {
      app_.report(Severity::Error, "bind_index_buffer: invalid or already-destroyed buffer handle");
      return;
    }
    SDL_GPUBufferBinding binding{.buffer = *handle, .offset = 0};
    SDL_BindGPUIndexBuffer(render_pass_, &binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  }

  auto SDL3Renderer::push_uniforms(PassHandle pass, ShaderStage stage, u32 slot, std::span<const u8> bytes) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "push_uniforms: pass handle does not match the current pass");
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

  auto SDL3Renderer::draw(PassHandle pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "draw: pass handle does not match the current pass");
      return;
    }
    SDL_DrawGPUPrimitives(render_pass_, vertex_count, instance_count, first_vertex, 0);
  }

  auto SDL3Renderer::draw_indexed(PassHandle pass, u32 index_count, u32 instance_count, u32 first_index) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "draw_indexed: pass handle does not match the current pass");
      return;
    }
    SDL_DrawGPUIndexedPrimitives(render_pass_, index_count, instance_count, first_index, 0, 0);
  }

  auto SDL3Renderer::end_pass(PassHandle pass) -> void
  {
    if (pass == PassHandle::Invalid || static_cast<u64>(pass) != pass_token_ || render_pass_ == nullptr)
    {
      app_.report(Severity::Error, "end_pass: pass handle does not match the current pass");
      return;
    }
    SDL_EndGPURenderPass(render_pass_);
    render_pass_ = nullptr;
  }
} // namespace engine
