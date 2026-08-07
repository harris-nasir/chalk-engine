# 0002: Split engine.renderer into contract and backend

## Status

Accepted (2026-08-07)
Supersedes: 0001-defer-renderer-audio-contracts.md (renderer half only; the
audio half of that deferral is untouched: engine.audio still has one
backend, XAudio2)

## Context

ADR-0001 deferred extracting a contract module for `engine.renderer`
until a second renderer backend was actually being built, on the
heuristic that guessing a contract's shape before a second implementation
exists means guessing what it needs to carry. That condition is now met:
`docs/design/2026-08-06-renderer-contract-design.md` designed
a primitive device-surface contract (buffers, textures, shaders,
pipelines, frame/pass/draw calls) and the SDL3 GPU backend implementing
it now exists alongside the pre-existing DX11 module.

## Decision

`engine.renderer` (primary module) now re-exports `engine.renderer.types`
(the contract: handles, description structs, the `RendererBackend`
concept) and aliases `engine::Renderer` to whichever backend
`CHALK_RENDERER` selects at compile time. `engine.renderer.sdl3` is the
first backend to actually implement the contract.

DX11's own migration to `RendererBackend` is separate follow-up work:
`renderer.dx11.ixx` still clears the screen with ad hoc D3D11 calls, not
through the contract. Selecting `CHALK_RENDERER=DX11` fails to build on
purpose (a clear `#error`) until that migration happens, rather than
silently building something that doesn't implement the contract.

## Consequences

- Future renderer backends (Vulkan, DX12, a software rasterizer) are a
  new backend module implementing `RendererBackend`, plus one new
  `#elif` branch in `renderer.ixx`, no contract changes.
- DX11's contract migration should follow the same shape SDL3's backend
  established: opaque `u64` handles behind a table per resource type,
  `App&` captured at construction for `app.report`, matching
  `SDL3Renderer`'s method signatures exactly (the concept enforces this
  at compile time regardless). One trap to avoid: `renderer.dx11.ixx`
  currently does `import engine.renderer;` (the primary module) for
  `Window`/`NativeWindowHandle`. Migrating `RendererDX11Plugin` to build
  a real `DX11Renderer` satisfying `RendererBackend` means the backend
  module must import `engine.renderer.types` instead, the same way
  `engine.renderer.sdl3` does, not the primary `engine.renderer` module,
  which imports the backend in its `CHALK_RENDERER_DX11` branch.
  Importing the primary from the backend would be a module cycle.
- A future architecture review should not re-flag the renderer half of
  ADR-0001 as unaddressed; it's resolved, not an oversight. The audio
  half (`engine.audio`/XAudio2) is untouched by this decision.
