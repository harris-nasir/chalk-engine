# SDL3 renderer contract implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `engine.renderer` as a real primitive contract (buffers, textures, shaders, pipelines, frame/pass/draw calls) and the SDL3 GPU backend that satisfies it, replacing today's `SDL_Renderer` 2D scratch code.

**Architecture:** A dependency-free types module (`engine.renderer.types`) defines every handle/description struct plus the `RendererBackend` concept. A concrete class `SDL3Renderer` (in `engine.renderer.sdl3`) implements every function the concept requires, using SDL3's `SDL_gpu.h` API. The primary `engine.renderer` module re-exports the types and does a compile-time `#if`/`#elif` on a CMake-supplied macro to pick a backend module and alias `engine::Renderer` to its concrete type — zero runtime dispatch, resolved at compile time.

**Tech Stack:** C++26 modules, SDL3 GPU (`SDL3::SDL3`, already CPM-fetched at `external/sdl3/`), CMake `CHALK_RENDERER` option.

**Why SDL3 only, for now:** the design doc names DX11 as the second backend that proves the contract is backend-neutral, and later Vulkan/DX12/software backends. This plan builds only the SDL3 GPU backend — the contract shape is exactly what makes adding Vulkan or DX12 later a new backend module plus a new `#elif` branch, no contract changes. DX11's own contract migration is separate follow-up work (its current file only clears the screen with ad hoc D3D11 calls, not the new contract) — flipping `CHALK_RENDERER` to `DX11` after this plan will fail to compile on purpose (loud `#error`, not a silent wrong build) until that follow-up happens.

## Global Constraints

These carry into every task below, copied from `docs/superpowers/specs/2026-08-06-renderer-contract-design.md` (status: approved):

- No inheritance, no virtual dispatch, no runtime type erasure anywhere in the contract's call path. `RendererBackend` is a `concept`; `SDL3Renderer` is a plain class satisfying it, not implementing an interface.
- Render-to-texture must be structurally possible from day one (it is: `RenderPassDescription` takes any `TextureHandle`, not just the swapchain).
- Vertex layout is backend-agnostic (byte offsets + format enums).
- Misuse (wrong call order, stale/destroyed handle) reports through `app.report(Severity::Error, ...)` and never crashes or UB's. Never call `Diagnostics` directly from a backend — always go through `App::report`.
- No abbreviations in type names unless industry-standard (`GPU`, `RGBA`).
- Every `create_*` has a matching `destroy_*`.

**One necessary correction to the approved spec's module layout:** the spec describes `engine.renderer:types` as a *partition* of `engine.renderer`, imported directly by backend modules. That is not legal C++: a module partition (`import M:part;`) is only importable from translation units that are themselves part of module `M` — a foreign module like `engine.renderer.sdl3` cannot name it (verified empirically against this project's clang 22 toolchain; `import A:types;` from a different module is a hard parse error, "expected ';' after module name"). If the primary `engine.renderer` also imports the backend module (needed for the compile-time `using Renderer = ...` alias), that's a cyclic module dependency, which modules also forbid. This plan makes `:types` a standalone sibling module, `engine.renderer.types`, instead of a partition. The intent (backend modules depend only on the data types, zero backend dependency in the types themselves, primary module does the compile-time switch) is fully preserved — only the on-disk module boundary changes. No ADR needed for this; it's a compiler-forced mechanical fix, not a design decision.

**Verification approach:** this project has no unit test framework (confirmed: no test files anywhere in the repo). Existing renderer work (the DX11 "clear screen" milestone) was verified by clean build + running the app. This plan follows the same pattern: each task's gate is "the build compiles" and, where a `static_assert` is the natural check (concept satisfaction), that. The final task's gate is "run the app, see the window clear via the GPU path."

**Shader format:** `SDL_CreateGPUDevice` is asked for `SDL_GPU_SHADERFORMAT_SPIRV` (routes through SDL3 GPU's Vulkan backend on Windows). No shader bytecode is compiled or loaded in this plan — the design doc explicitly defers a shader pipeline until real shader variety exists (`Deferred` section), and this plan's only runtime-exercised path is a clear-only render pass. `create_shader`/`create_pipeline` are implemented for real (required by the concept) but aren't called by anything yet, same status as `create_buffer`/`create_texture` today.

---

### Task 1: `engine.renderer.types` — contract data types and concept

**Files:**
- Create: `engine/plugin/renderer/renderer.types.ixx`

**Interfaces:**
- Produces (consumed by Task 2, 3, 4, 5): `Color`, `BufferHandle`/`TextureHandle`/`ShaderHandle`/`PipelineHandle`/`FrameHandle`/`PassHandle`/`CopyPassHandle` (each `enum class : u64 { Invalid = 0 }`), `BufferUsage`, `BufferDescription{u64 size; BufferUsage usage;}`, `PixelFormat`, `TextureUsage`, `TextureDescription{u32 width, height; PixelFormat format; TextureUsage usage;}`, `VertexFormat`, `VertexAttribute{u32 location, offset; VertexFormat format;}`, `Topology`, `BlendState{bool enabled;}`, `DepthState{bool test_enabled, write_enabled;}`, `ShaderStage`, `ShaderSource{span<const u8> bytecode; ShaderStage stage; const char* entry_point; u32 sampler_count, storage_texture_count, storage_buffer_count, uniform_buffer_count;}`, `PipelineDescription{ShaderHandle vertex_shader, fragment_shader; span<const VertexAttribute> vertex_layout; u32 vertex_stride; Topology topology; BlendState blend; DepthState depth;}`, `LoadOp`, `StoreOp`, `ColorAttachment{TextureHandle target; LoadOp load; StoreOp store; Color clear;}`, `DepthAttachment{TextureHandle target; LoadOp load; StoreOp store; f32 clear_depth;}`, `RenderPassDescription{span<const ColorAttachment> color_attachments; Option<DepthAttachment> depth_attachment;}`, concept `RendererBackend<T>`.

Note: `ShaderSource` carries more fields than the design doc's one-liner sketch (`{ std::span<const u8> bytecode; }`). SDL's `SDL_GPUShaderCreateInfo` genuinely needs a stage, entry point, and resource-binding counts to create a shader at all — the doc's sketch was illustrative, not exhaustive. Adding fields doesn't change the concept's required signature (`create_shader(ShaderSource)` still matches `ShaderSource{}`), so this doesn't violate the locked contract shape.

- [ ] **Step 1: Write the module**

```cpp
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
  enum class FrameHandle : u64
  {
    Invalid = 0
  };
  enum class PassHandle : u64
  {
    Invalid = 0
  };
  enum class CopyPassHandle : u64
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
    DontCare,
  };

  enum class StoreOp : u8
  {
    Store,
    DontCare,
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
    Option<DepthAttachment> depth_attachment; // empty = no depth buffer bound
  };

  template <typename T>
  concept RendererBackend = requires(
      T& r, BufferDescription buffer_description, TextureDescription texture_description, ShaderSource shader_source,
      PipelineDescription pipeline_description, RenderPassDescription pass_description, BufferHandle buffer,
      TextureHandle texture, ShaderHandle shader, PipelineHandle pipeline, FrameHandle frame, PassHandle pass,
      CopyPassHandle copy_pass, ShaderStage stage, u32 count, std::span<const u8> bytes
  ) {
    { r.create_buffer(buffer_description) } -> std::same_as<BufferHandle>;
    { r.destroy_buffer(buffer) } -> std::same_as<void>;
    { r.create_texture(texture_description) } -> std::same_as<TextureHandle>;
    { r.destroy_texture(texture) } -> std::same_as<void>;
    { r.create_shader(shader_source) } -> std::same_as<ShaderHandle>;
    { r.destroy_shader(shader) } -> std::same_as<void>;
    { r.create_pipeline(pipeline_description) } -> std::same_as<PipelineHandle>;
    { r.destroy_pipeline(pipeline) } -> std::same_as<void>;

    { r.begin_frame() } -> std::same_as<FrameHandle>;
    { r.swapchain_texture(frame) } -> std::same_as<TextureHandle>;
    { r.begin_copy_pass(frame) } -> std::same_as<CopyPassHandle>;
    { r.upload_buffer(copy_pass, buffer, bytes) } -> std::same_as<void>;
    { r.upload_texture(copy_pass, texture, bytes, count) } -> std::same_as<void>;
    { r.end_copy_pass(copy_pass) } -> std::same_as<void>;

    { r.begin_pass(frame, pass_description) } -> std::same_as<PassHandle>;
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
```

- [ ] **Step 2: Build to verify it compiles**

Run: `cmake --build --preset clang`
Expected: succeeds (nothing imports this module yet, so this only checks the file itself parses and its types/concept are well-formed).

- [ ] **Step 3: Commit**

```bash
git add engine/plugin/renderer/renderer.types.ixx
git commit -m "feature(renderer): add engine.renderer.types contract module"
```

---

### Task 2: `SDL3Renderer` — resource creation (buffers, textures, shaders, pipelines)

**Files:**
- Modify: `engine/plugin/renderer/sdl3/renderer.sdl3.ixx` (existing file; this task adds new code alongside the current `SDL_Renderer`-2D code, which Task 5 deletes — the file must keep compiling at every step)

**Interfaces:**
- Consumes: everything from Task 1 (`engine.renderer.types`).
- Produces (consumed by Task 3, 5): class `SDL3Renderer` with a constructor `SDL3Renderer(App& app, SDL_GPUDevice* device, SDL_Window* window)`, and (this task) `create_buffer`, `destroy_buffer`, `create_texture`, `destroy_texture`, `create_shader`, `destroy_shader`, `create_pipeline`, `destroy_pipeline`. Task 3 adds the rest of the concept's functions to the same class. Private helper template `HandleTable<T>` (anonymous namespace, not exported).

- [ ] **Step 1: Add the new imports the file will need**

At the top of `engine/plugin/renderer/sdl3/renderer.sdl3.ixx`, change:

```cpp
module;

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

export module engine.renderer.sdl3;

import engine.core;
import engine.platform;
import engine.renderer;
```

to:

```cpp
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
```

(`engine.platform` stays for now — the old code in this file still uses `NativeWindowHandle`/`Window` until Task 5 deletes it. `engine.renderer` becomes `engine.renderer.types`: the backend must not import the primary `engine.renderer` module, since that module will import this one back in Task 4 — a cycle. See the Global Constraints note on the spec correction.)

- [ ] **Step 2: Add the handle table and SDL mapping helpers**

Add to the existing anonymous namespace in the file (the one already holding `State` and `Texture`) — append these below the existing `Texture` struct:

```cpp
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
  };

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
```

- [ ] **Step 3: Add the `SDL3Renderer` class with resource-management methods**

Add this new `export namespace engine { ... }` block to the file, after the existing `RendererSDL3Plugin` class declaration (the one with just `void build(App& app);`) and before the anonymous namespace, i.e. as a second exported class in the same export block or a new one immediately after it:

```cpp
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

  private:
    App& app_;
    SDL_GPUDevice* device_;
    SDL_Window* window_;

    HandleTable<SDL_GPUBuffer*> buffers_;
    HandleTable<TextureRecord> textures_;
    HandleTable<SDL_GPUShader*> shaders_;
    HandleTable<SDL_GPUGraphicsPipeline*> pipelines_;
  };

} // namespace engine
```

Then implement the methods in the file's existing `namespace engine { ... }` definition block (where `RendererSDL3Plugin::build` is defined), adding these member function definitions:

```cpp
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
    return static_cast<TextureHandle>(
        textures_.insert(TextureRecord{.handle = texture, .width = description.width, .height = description.height})
    );
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
    // (PipelineDescription doesn't carry a target format yet — no caller
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
```

- [ ] **Step 4: Build to verify it compiles**

Run: `cmake --build --preset clang`
Expected: succeeds. `SDL3Renderer` doesn't satisfy `RendererBackend` yet (frame/pass functions missing) — that's fine, nothing asserts the concept until Task 3.

- [ ] **Step 5: Commit**

```bash
git add engine/plugin/renderer/sdl3/renderer.sdl3.ixx
git commit -m "feature(renderer:sdl3): add SDL3Renderer resource creation (buffers, textures, shaders, pipelines)"
```

---

### Task 3: `SDL3Renderer` — frame lifecycle, copy passes, render passes, draw calls

**Files:**
- Modify: `engine/plugin/renderer/sdl3/renderer.sdl3.ixx` (same file as Task 2)

**Interfaces:**
- Consumes: `SDL3Renderer` from Task 2 (same class, same file).
- Produces (consumed by Task 4, 5): the rest of `RendererBackend`'s functions on `SDL3Renderer`: `begin_frame`, `swapchain_texture`, `begin_copy_pass`, `upload_buffer`, `upload_texture`, `end_copy_pass`, `begin_pass`, `bind_pipeline`, `bind_vertex_buffer`, `bind_index_buffer`, `push_uniforms`, `draw`, `draw_indexed`, `end_pass`, `submit`. After this task, `static_assert(RendererBackend<SDL3Renderer>);` compiles.

- [ ] **Step 1: Add frame-state private members to `SDL3Renderer`**

In the class body added in Task 2, extend the private section:

```cpp
  private:
    [[nodiscard]] auto resolve_texture(TextureHandle handle) -> SDL_GPUTexture*;

    App& app_;
    SDL_GPUDevice* device_;
    SDL_Window* window_;

    HandleTable<SDL_GPUBuffer*> buffers_;
    HandleTable<TextureRecord> textures_;
    HandleTable<SDL_GPUShader*> shaders_;
    HandleTable<SDL_GPUGraphicsPipeline*> pipelines_;

    u64 frame_token_                        = 0;
    SDL_GPUCommandBuffer* frame_command_buffer_ = nullptr;
    SDL_GPUTexture* frame_swapchain_texture_    = nullptr;

    u64 copy_pass_token_        = 0;
    SDL_GPUCopyPass* copy_pass_ = nullptr;

    u64 pass_token_               = 0;
    SDL_GPURenderPass* render_pass_ = nullptr;
```

And add the public method declarations (in the public section, after the Task 2 declarations):

```cpp
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
```

Add the sentinel constant to the anonymous namespace (near `TextureRecord`):

```cpp
  // Never a real HandleTable index (those start at 1 and grow one at a
  // time), so it can never collide with a created texture's handle.
  constexpr u64 SWAPCHAIN_TEXTURE_HANDLE = ~u64{0};
```

- [ ] **Step 2: Implement frame lifecycle and `resolve_texture`**

Add to the `namespace engine { ... }` definition block:

```cpp
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
    if (!SDL_SubmitGPUCommandBuffer(frame_command_buffer_))
    {
      app_.report(Severity::Error, "SDL_SubmitGPUCommandBuffer failed: {}", SDL_GetError());
    }
    frame_command_buffer_    = nullptr;
    frame_swapchain_texture_ = nullptr;
  }
```

- [ ] **Step 3: Implement copy pass functions**

```cpp
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
        .pixels_per_row  = bytes_per_row,
        .rows_per_layer  = record->height,
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
```

- [ ] **Step 4: Implement render pass and draw functions**

```cpp
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
```

- [ ] **Step 5: Add the concept static_assert**

Immediately after the `SDL3Renderer` class definition (the `export namespace engine { class SDL3Renderer { ... }; ... }` block from Task 2/3), add:

```cpp
export namespace engine
{
  static_assert(RendererBackend<SDL3Renderer>);
}
```

(Can also go as a plain non-exported `static_assert(engine::RendererBackend<engine::SDL3Renderer>);` right after the class if placing it inside a second `export namespace` block feels redundant — either compiles the same check. Pick whichever reads cleaner once the file is in front of you.)

- [ ] **Step 6: Build to verify it compiles**

Run: `cmake --build --preset clang`
Expected: succeeds. This is the real gate for this task — if any function signature doesn't match `RendererBackend`, the `static_assert` fails with a clear "constraint not satisfied" diagnostic pointing at the mismatched function.

- [ ] **Step 7: Commit**

```bash
git add engine/plugin/renderer/sdl3/renderer.sdl3.ixx
git commit -m "feature(renderer:sdl3): complete SDL3Renderer contract (frame, pass, draw) and verify concept"
```

---

### Task 4: Compile-time backend selection (`CHALK_RENDERER` CMake option + `engine.renderer` dispatch)

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `engine/plugin/renderer/renderer.ixx`

**Interfaces:**
- Consumes: `SDL3Renderer` (Task 3), `engine.renderer.types` (Task 1).
- Produces (consumed by Task 5): `engine::Renderer` (alias to `SDL3Renderer` when `CHALK_RENDERER=SDL3`), re-exported `engine.renderer.types` contents, all reachable via `import engine.renderer;`.

- [ ] **Step 1: Add the CMake option**

In `CMakeLists.txt`, after line 6 (`set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`) and before line 8 (`set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ...)`), add:

```cmake
set(CHALK_RENDERER "SDL3" CACHE STRING "Renderer backend selected at compile time")
set_property(CACHE CHALK_RENDERER PROPERTY STRINGS SDL3 DX11)
```

- [ ] **Step 2: Wire the compile definition**

After line 45 (`set_project_warnings(${PROJECT_NAME})`) and before the `if(MSVC)` block, add:

```cmake
if(CHALK_RENDERER STREQUAL "SDL3")
  target_compile_definitions(${PROJECT_NAME} PRIVATE CHALK_RENDERER_SDL3)
elseif(CHALK_RENDERER STREQUAL "DX11")
  target_compile_definitions(${PROJECT_NAME} PRIVATE CHALK_RENDERER_DX11)
else()
  message(FATAL_ERROR "Unknown CHALK_RENDERER value: ${CHALK_RENDERER} (expected SDL3 or DX11)")
endif()
```

- [ ] **Step 3: Rewrite `engine.renderer`'s primary interface**

Replace the entire contents of `engine/plugin/renderer/renderer.ixx`:

```cpp
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
```

(The trailing `static_assert` is outside any namespace, using fully-qualified names — it re-checks the alias resolves to something satisfying the contract, which is otherwise implicit. It's a real, permanent check: if a future backend module gets aliased here without actually finishing its `RendererBackend` implementation, this is where the build fails, with a message naming this exact line.)

- [ ] **Step 4: Reconfigure and build**

Run: `cmake --preset clang` then `cmake --build --preset clang`
Expected: configure regenerates cleanly (new cache variable `CHALK_RENDERER` defaults to `SDL3`); build compiles. `engine.ixx`'s facade (`export import engine.renderer;`) needs no change — it already re-exports the primary module, which now carries more.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt engine/plugin/renderer/renderer.ixx
git commit -m "feature(renderer): compile-time backend selection via CHALK_RENDERER"
```

---

### Task 5: Wire `RendererSDL3Plugin` to the contract, delete the old 2D scratch code

**Files:**
- Modify: `engine/plugin/renderer/sdl3/renderer.sdl3.ixx`

**Interfaces:**
- Consumes: `engine::Renderer` (= `SDL3Renderer`) from Task 4, `SDL_Window*` resource (already inserted by `PlatformSDL3Plugin`, see `engine/plugin/platform/sdl3/platform.sdl3.ixx:60`).
- Produces: `RendererSDL3Plugin` (unchanged public shape: `void build(App&)`) now creates a real `SDL_GPUDevice`, claims the platform's window, inserts an `engine::Renderer` resource, and clears the screen through the new contract every frame.

- [ ] **Step 1: Delete the old `State`/`Texture` code and the old `RendererSDL3Plugin::build`**

In `engine/plugin/renderer/sdl3/renderer.sdl3.ixx`:

- Delete the anonymous-namespace `State` and `Texture` structs (the ones with `SDL_Renderer* renderer`, `SDL_Texture* handle`, etc — not `HandleTable`/`TextureRecord`/the mapping helpers from Task 2/3, which stay).
- Delete the entire existing `void RendererSDL3Plugin::build(App& app) { ... }` body (five `app.add_system(...)` calls: window-wrapper + `SDL_CreateRenderer` Startup, texture-load Startup, position Update, clear+present Render, two Shutdown systems).
- Remove the `#include <SDL3_image/SDL_image.h>` line (no longer used) and the `import engine.platform;` line (no longer used — the new code only touches `SDL_Window*`, not `Window`/`NativeWindowHandle`).

- [ ] **Step 2: Write the new `RendererSDL3Plugin::build`**

Add in its place, inside the file's `namespace engine { ... }` definition block:

```cpp
  void RendererSDL3Plugin::build(App& app)
  {
    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          SDL_Window* window = app.require_resource<SDL_Window*>();

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

          app.insert_resource<Renderer>(Renderer{app, device, window});
          app.report(Severity::Info, "SDL3 GPU renderer ready");
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<Renderer>();

          FrameHandle frame  = renderer.begin_frame();
          TextureHandle target = renderer.swapchain_texture(frame);
          if (target == TextureHandle::Invalid)
          {
            return; // window minimized or the frame failed to acquire; nothing to draw this frame
          }

          std::array<ColorAttachment, 1> color_attachments{ColorAttachment{
              .target = target,
              .load   = LoadOp::Clear,
              .store  = StoreOp::Store,
              .clear  = Color{.r = 17.0f / 255.0f, .g = 17.0f / 255.0f, .b = 17.0f / 255.0f, .a = 1.0f},
          }};

          PassHandle pass = renderer.begin_pass(frame, RenderPassDescription{.color_attachments = color_attachments});
          renderer.end_pass(pass);
          renderer.submit(frame);
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<Renderer>();
          SDL_ReleaseWindowFromGPUDevice(renderer.device(), renderer.window());
          SDL_DestroyGPUDevice(renderer.device());
          app.report(Severity::Info, "SDL3 GPU renderer shut down");
        }
    );
  }
```

Add `#include <array>` to the file's global module fragment (needed for `std::array` above).

- [ ] **Step 3: Build**

Run: `cmake --build --preset clang`
Expected: succeeds.

- [ ] **Step 4: Run and visually confirm**

Run: `./build/chalk.exe`
Expected: a window opens, titled "Engine", and clears to a dark grey (`rgb(17,17,17)`) every frame via the SDL3 GPU path — the same clear color the old `SDL_Renderer` 2D code used, now going through `begin_frame → begin_pass → end_pass → submit`. Console output shows `[renderer.sdl3][INFO] ... SDL3 GPU renderer ready` at startup and `... SDL3 GPU renderer shut down` on close (close the window to trigger shutdown). No `[ERROR]`/`[FATAL]` diagnostics.

- [ ] **Step 5: Commit**

```bash
git add engine/plugin/renderer/sdl3/renderer.sdl3.ixx
git commit -m "feature(renderer:sdl3): wire RendererSDL3Plugin to the contract, drop SDL_Renderer 2D scratch code"
```

---

### Task 6: Supersede ADR-0001

**Files:**
- Create: `docs/adr/0002-renderer-contract-second-backend.md`

This is the spec's own listed follow-up ("Supersede ADR-0001 with a new ADR recording that the deferral condition has been met"). ADR-0001 deferred splitting `engine.renderer` into a contract + backend split until a second backend was actually planned; the SDL3 GPU backend built in this plan (alongside the pre-existing DX11 module) is that second backend, even though DX11's own migration to the new contract is separate follow-up work.

- [ ] **Step 1: Write the ADR**

```markdown
# 0002: Split engine.renderer into contract and backend

## Status

Accepted (2026-08-07)
Supersedes: 0001-defer-renderer-audio-contracts.md (renderer half only; the
audio half of that deferral is untouched — engine.audio still has one
backend, XAudio2)

## Context

ADR-0001 deferred extracting a contract module for `engine.renderer`
until a second renderer backend was actually being built, on the
heuristic that guessing a contract's shape before a second implementation
exists means guessing what it needs to carry. That condition is now met:
`docs/superpowers/specs/2026-08-06-renderer-contract-design.md` designed
a primitive device-surface contract (buffers, textures, shaders,
pipelines, frame/pass/draw calls) and the SDL3 GPU backend implementing
it now exists alongside the pre-existing DX11 module.

## Decision

`engine.renderer` (primary module) now re-exports `engine.renderer.types`
(the contract: handles, description structs, the `RendererBackend`
concept) and aliases `engine::Renderer` to whichever backend
`CHALK_RENDERER` selects at compile time. `engine.renderer.sdl3` is the
first backend to actually implement the contract.

DX11's own migration to `RendererBackend` is separate follow-up work —
`renderer.dx11.ixx` still clears the screen with ad hoc D3D11 calls, not
through the contract. Selecting `CHALK_RENDERER=DX11` fails to build on
purpose (a clear `#error`) until that migration happens, rather than
silently building something that doesn't implement the contract.

## Consequences

- Future renderer backends (Vulkan, DX12, a software rasterizer) are a
  new backend module implementing `RendererBackend`, plus one new
  `#elif` branch in `renderer.ixx` — no contract changes.
- DX11's contract migration should follow the same shape SDL3's backend
  established: opaque `u64` handles behind a table per resource type,
  `App&` captured at construction for `app.report`, matching
  `SDL3Renderer`'s method signatures exactly (the concept enforces this
  at compile time regardless).
- A future architecture review should not re-flag the renderer half of
  ADR-0001 as unaddressed; it's resolved, not an oversight. The audio
  half (`engine.audio`/XAudio2) is untouched by this decision.
```

- [ ] **Step 2: Do not commit**

Per standing project preference, spec/design/ADR docs in this repo are left staged, not committed, as part of an automated plan workflow — leave this for the user to review and commit themselves.

---

## Self-review notes

- **Spec coverage:** contract module (Task 1), concept enforcement via `static_assert` (Task 3/4), compile-time backend switch via CMake (Task 4), SDL3 GPU backend implementing every function (Task 2/3), render-to-texture structurally supported (any `TextureHandle` works as a color/depth attachment target, not just the swapchain — Task 3's `begin_pass`), backend-agnostic vertex layout (byte offset + format enum, Task 1/2), misuse reporting via `app.report` never crash (every method in Task 2/3 validates its handles/tokens first). Sprite/text/camera/ECS are explicitly out of scope per the spec's own `Deferred` section — not built here. DX11 migration and a real shader pipeline are explicitly out of scope per this plan's stated scope — separate follow-up plans.
- **Placeholder scan:** no TODOs, no stub bodies — every function in Task 2/3 does real SDL3 GPU work, even the ones nothing calls yet this pass (`create_shader`, `create_pipeline`, `upload_buffer`, `upload_texture` — same status as `create_buffer`/`create_texture`, which have always been implemented before any caller exists).
- **Type/signature consistency:** checked `Renderer`/`SDL3Renderer`/`RendererSDL3Plugin` naming is consistent across Tasks 2–5; every `create_*`/`destroy_*` pair uses matching handle types; `HandleTable<T>::get`/`destroy` used consistently by handle value (`static_cast<u64>(handle)`) everywhere a handle is dereferenced.
