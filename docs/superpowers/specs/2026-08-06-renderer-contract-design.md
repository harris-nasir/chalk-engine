# Renderer contract: primitive device surface

Date: 2026-08-06
Status: approved, not yet implemented
Supersedes: `docs/plans/2026-08-06-renderer-contract-design.md` (the
`FrameCommands`/`std::any` command-list design — abandoned, see
`docs/plans/2026-08-06-renderer-contract-handoff.txt` section 2 for why).
Reopens: `docs/adr/0001-defer-renderer-audio-contracts.md` (deferral was
conditional on a second renderer backend being actually planned; the SDL3
backend now being built alongside DX11 is that second backend).

## Goal

Define `engine.renderer` as a CONTRACT every renderer backend implements:
SDL3 GPU today, DX11 second (to prove the contract is backend-neutral), a
software/CPU renderer later. The contract is the same primitive vocabulary
real GPU APIs share (buffers, textures, pipelines, passes, draw calls) — not
feature-level convenience functions (`draw_quad`, `draw_text`). Anything
built on top (sprites, text, meshes, cameras, post-processing) is a separate
plugin layered on these primitives, with zero contract changes needed. Every
backend must produce identical pixel output. The renderer contract does not
depend on or assume an ECS — that's game-side management, out of scope here.

End goal driving every decision: a performant, high-quality 3D racing
simulator (Assetto Corsa class). Render targets, depth testing, and
compile-time dispatch are in scope from day one because that end goal needs
them, not because they're generically nice to have.

## Locked constraints (carried over from the handoff)

- Game code never talks to the renderer directly and never pushes render
  calls itself; that boundary belongs to feature plugins layered on top of
  this contract (out of scope for this document).
- No inheritance, no virtual dispatch, no runtime erasure (fn-pointer table,
  `std::variant`, etc.) anywhere in the contract's own call path — both an
  architectural preference (matches the existing `Plugin` concept's
  duck-typing, not a base class) and a performance requirement (renderer
  calls should resolve at compile/link time, not runtime).
- Render-to-texture must work from day one — bloom/SSR/motion blur (needed
  for AC-class graphics) are render-to-texture techniques; adding
  pass-to-arbitrary-texture later would be a breaking contract change.
- Vertex layout is backend-agnostic: byte offsets + format enums, so DX11
  input layouts and Vulkan/SDL3 GPU vertex attributes both map cleanly.
- Misuse (wrong call order, stale handle) must report through `app.report`
  and never crash/UB — never call into `Diagnostics` directly, so
  `Diagnostics` can be replaced or extended later without touching call
  sites.
- No abbreviations in type names unless the abbreviation is an industry
  standard (e.g. `GPU`, `RGBA`). `Description`, not `Desc`.

## Module layout

- `engine.renderer:types` (partition) — pure data: opaque handles,
  description structs, the `RendererBackend` concept. No backend
  dependency; this is what backend modules import to shape their concrete
  class.
- `engine.renderer` (primary interface) — the compile-time backend switch.
  Imports exactly one backend module, selected by a CMake compile
  definition (`CHALK_RENDERER=SDL3` or `DX11`), and exports
  `using Renderer = <chosen backend type>;` plus re-exports the `:types`
  partition. This is a deliberate, documented departure from the
  contract-module pattern described in `CONTEXT.md` ("no backend code") —
  the tradeoff is explicit: zero runtime dispatch, at the cost of only one
  backend linking into a given binary.

## Backend selection mechanism

Today, which backend runs is a source edit in `plugin.default.ixx`
(comment/uncomment `add_plugin<RendererSDL3Plugin>` vs
`add_plugin<RendererDX11Plugin>`), with both backend modules always
compiled into the one executable target (`CMakeLists.txt` globs every
`.ixx` under `engine/`). The new mechanism moves that choice to a CMake
option (`CHALK_RENDERER`, default `SDL3`, one-line switch to `DX11`),
passed as a compile definition. `engine.renderer`'s primary interface unit
does the `#if`/`#elif` on that define to import the matching backend
module. Consequence: every module that does `import engine.renderer;`
resolves `engine::Renderer` to one concrete type, fixed before any
feature-plugin module compiles — no runtime branching anywhere in the call
path.

Cost accepted: comparing SDL3 vs DX11 pixel output means building both
CMake configurations and comparing output offline, not an in-process
side-by-side. Acceptable because chalk is fully source-recompiled per
preset already, not a stable-ABI plugin system shipping prebuilt binaries.

## Concept enforcement

`RendererBackend` (in `:types`) constrains a plain concrete class's shape —
no base class, no `override`. Each backend struct gets
`static_assert(engine::RendererBackend<SDL3Renderer>);` in its own file: a
missing or wrong-shaped function is a compile error at the backend's own
file:line, not a distant, confusing one.

## Contract API

```cpp
// engine.renderer:types

enum class BufferHandle   : u64 { Invalid = 0 };
enum class TextureHandle  : u64 { Invalid = 0 };
enum class ShaderHandle   : u64 { Invalid = 0 };
enum class PipelineHandle : u64 { Invalid = 0 };
enum class FrameHandle    : u64 { Invalid = 0 };
enum class PassHandle     : u64 { Invalid = 0 };
enum class CopyPassHandle : u64 { Invalid = 0 };

enum class BufferUsage : u8 { Vertex, Index, Uniform };
struct BufferDescription { u64 size; BufferUsage usage; };

enum class PixelFormat  : u8 { RGBA8, BGRA8, Depth24Stencil8 };
enum class TextureUsage : u32 { Sampled = 1 << 0, ColorTarget = 1 << 1, DepthTarget = 1 << 2 };
struct TextureDescription { u32 width, height; PixelFormat format; TextureUsage usage; };

enum class VertexFormat : u8 { F32, F32x2, F32x3, F32x4, U8x4Norm };
struct VertexAttribute { u32 location; u32 offset; VertexFormat format; };
enum class Topology : u8 { TriangleList, LineList, PointList };
struct BlendState { bool enabled; };
struct DepthState  { bool test_enabled; bool write_enabled; };

// Bytecode content is backend-specific (SPIR-V/DXIL/MSL for SDL3 GPU,
// DXBC for DX11), supplied as a per-backend asset file selected by the
// same CHALK_RENDERER switch that picks the backend module. No shader
// cross-compiler in the build (deferred — see Deferred section).
struct ShaderSource { std::span<const u8> bytecode; };

struct PipelineDescription
{
  ShaderHandle vertex_shader, fragment_shader;
  std::span<const VertexAttribute> vertex_layout;
  u32 vertex_stride;
  Topology topology;
  BlendState blend;
  DepthState depth;
};

enum class LoadOp  : u8 { Clear, Load, DontCare };
enum class StoreOp : u8 { Store, DontCare };
struct ColorAttachment { TextureHandle target; LoadOp load; StoreOp store; Color clear; };
struct DepthAttachment { TextureHandle target; LoadOp load; StoreOp store; f32 clear_depth; };
struct RenderPassDescription
{
  std::span<const ColorAttachment> color_attachments;
  Option<DepthAttachment> depth_attachment; // empty = no depth buffer bound
};

enum class ShaderStage : u8 { Vertex, Fragment };

template <typename T>
concept RendererBackend = requires(T& r, /* ...every description/handle/span parameter... */) {
  { r.create_buffer(BufferDescription{}) }      -> std::same_as<BufferHandle>;
  { r.destroy_buffer(BufferHandle{}) }          -> std::same_as<void>;
  { r.create_texture(TextureDescription{}) }    -> std::same_as<TextureHandle>;
  { r.destroy_texture(TextureHandle{}) }        -> std::same_as<void>;
  { r.create_shader(ShaderSource{}) }           -> std::same_as<ShaderHandle>;
  { r.destroy_shader(ShaderHandle{}) }          -> std::same_as<void>;
  { r.create_pipeline(PipelineDescription{}) }  -> std::same_as<PipelineHandle>;
  { r.destroy_pipeline(PipelineHandle{}) }      -> std::same_as<void>;

  { r.begin_frame() }                      -> std::same_as<FrameHandle>;
  { r.swapchain_texture(FrameHandle{}) }   -> std::same_as<TextureHandle>;
  { r.begin_copy_pass(FrameHandle{}) }     -> std::same_as<CopyPassHandle>;
  { r.upload_buffer(CopyPassHandle{}, BufferHandle{}, std::span<const u8>{}) }          -> std::same_as<void>;
  { r.upload_texture(CopyPassHandle{}, TextureHandle{}, std::span<const u8>{}, u32{}) } -> std::same_as<void>;
  { r.end_copy_pass(CopyPassHandle{}) }    -> std::same_as<void>;

  { r.begin_pass(FrameHandle{}, RenderPassDescription{}) }      -> std::same_as<PassHandle>;
  { r.bind_pipeline(PassHandle{}, PipelineHandle{}) }           -> std::same_as<void>;
  { r.bind_vertex_buffer(PassHandle{}, BufferHandle{}, u32{}) } -> std::same_as<void>;
  { r.bind_index_buffer(PassHandle{}, BufferHandle{}) }         -> std::same_as<void>;
  { r.push_uniforms(PassHandle{}, ShaderStage{}, u32{}, std::span<const u8>{}) } -> std::same_as<void>;
  { r.draw(PassHandle{}, u32{}, u32{}, u32{}) }         -> std::same_as<void>;
  { r.draw_indexed(PassHandle{}, u32{}, u32{}, u32{}) } -> std::same_as<void>;
  { r.end_pass(PassHandle{}) }             -> std::same_as<void>;
  { r.submit(FrameHandle{}) }              -> std::same_as<void>;
};
```

Notes on shape:

- `push_uniforms` takes `ShaderStage` explicitly because SDL3 GPU has
  separate vertex/fragment uniform push calls and DX11 separates
  `VSSetConstantBuffers`/`PSSetConstantBuffers` — a bare slot number can't
  disambiguate which stage without it.
- Uploads get their own copy-pass pair (`begin_copy_pass`/`upload_buffer`/
  `upload_texture`/`end_copy_pass`), symmetric with the render pass,
  because SDL3 GPU requires uploads inside a distinct copy pass
  (`SDL_BeginGPUCopyPass`) and cannot interleave them with a render pass.
  DX11 has no such restriction; its backend just performs the upload
  immediately inside these calls and ignores the pass boundary — the
  contract models the stricter backend correctly, the looser one absorbs
  the extra structure for free.
- `swapchain_texture(FrameHandle)` returns the acquired backbuffer as a
  plain `TextureHandle`, so `RenderPassDescription.color_attachments` never
  needs a separate "is this the screen" case — a pass targeting the window
  and a pass targeting an offscreen texture are the same call shape. This
  is what makes multi-pass techniques (a pass rendering to a texture later
  sampled by a second pass) buildable later with zero contract changes.
- Every `create_*` has a matching `destroy_*`, called from `Shutdown`
  systems (mirrors the existing reversed-Shutdown-order pattern) and,
  later, resource reload.
- Ordering misuse (`draw` outside a pass, a `PassHandle` reused after
  `end_pass`, `submit` without a matching `begin_frame`) must report
  `Severity::Error` and never crash/UB. The concrete backend struct
  captures the owning `App&` at Startup (same moment `State` resources are
  built today) and calls `app.report(...)` — never `Diagnostics` directly —
  so `Diagnostics` can be replaced or extended without touching any call
  site in a backend.

## Deferred (do not drag back in)

- A universal/cross-compiled shader language. Feature set stays tiny for
  now (flat-color unlit primitives); shaders are hand-written per backend
  as plain asset files, selected by the same `CHALK_RENDERER` switch.
  Revisit once real shader variety exists.
- Sprites, text, cameras, meshes, post-processing, compute — all future
  plugins built on these primitives. They prove the contract's
  extensibility; they must not shape it, and this document deliberately
  contains no sprite/camera examples for that reason.
- ECS: game-side, orthogonal to this contract.
- Runtime backend switching / both backends in one binary — accepted cost
  of compile-time selection, see Backend selection mechanism above.

## Follow-ups (outside this document)

- Implement `engine.renderer:types` + primary interface + CMake option.
- Implement the SDL3 GPU backend (`engine.renderer.sdl3`, rewritten from
  today's SDL_Renderer-2D scratch code) and the DX11 backend against the
  same contract.
- Supersede ADR-0001 with a new ADR recording that the deferral condition
  (second backend actually being planned) has been met.
- A feature-plugin layer (sprite, text, etc.) and a substantial game to
  validate the contract end to end — separate design work, not part of
  this contract.
