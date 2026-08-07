# Architecture

## Layout

```
engine/
  engine.ixx                                module engine                  : facade, re-exports every contract
  core/core.ixx                             module engine.core             : App, Schedule, resources, systems
  plugin/
    platform/platform.ixx                   module engine.platform         : WindowDescription / Window / NativeWindowHandle
    platform/sdl3/platform.sdl3.ixx         module engine.platform.sdl3    : PlatformSDL3Plugin
    platform/win32/platform.win32.ixx       module engine.platform.win32   : PlatformWin32Plugin (stub, unused)
    renderer/renderer.types.ixx             module engine.renderer.types   : handles, description structs, RendererBackend concept
    renderer/renderer.ixx                   module engine.renderer         : compile-time backend selection, Renderer alias
    renderer/sdl3/renderer.sdl3.ixx         module engine.renderer.sdl3    : SDL3Renderer (implements RendererBackend), RendererSDL3Plugin
    renderer/dx11/renderer.dx11.ixx         module engine.renderer.dx11    : RendererDX11Plugin (not yet migrated to the contract)
    audio/audio.ixx                         module engine.audio
    audio/xaudio2/audio.xaudio2.ixx         module engine.audio.xaudio2    : AudioXAudio2Plugin
    input/input.ixx                         module engine.input            : InputState
    input/xinput/input.xinput.ixx           module engine.input.xinput     : InputXInputPlugin
    physics/physics.ixx                     module engine.physics          : PhysicsWorld
    physics/simple/physics.simple.ixx       module engine.physics.simple   : PhysicsSimplePlugin
source/
  main.cxx                                  : composition root
```

Each subsystem has two modules: a **contract** with the shared types, and **backends** that implement it. `engine.ixx` re-exports core and every contract; it never imports a backend.

## Concepts

- **Resources**: one instance of any type, addressed by type: `app.insert_resource<T>({})`, `app.resource<T>()` (returns an `Option<T>` handle).
- **Systems**: plain functions `void(App&)` run each frame in pipeline order (`Startup`, `PreUpdate`, `FixedUpdate`, `Update`, `PostUpdate`, `Render`, `Shutdown`).
- **Plugins**: any type with `void build(App&)` that registers resources and systems.

`source/main.cxx` is the composition root: it imports the concrete backend modules and adds their plugins in dependency order. Swapping a backend is editing one import line and one plugin name, or, for the renderer, one CMake option.

## The renderer contract

`engine.renderer.types` defines the renderer as a primitive device surface: opaque handles (buffers, textures, shaders, pipelines), a frame/pass/draw call vocabulary, the same shape real GPU APIs share, not feature-level helpers like `draw_quad`. A backend is any class satisfying the `RendererBackend` concept: no inheritance, no virtual dispatch, checked at compile time with `static_assert`.

Which backend actually runs is picked at configure time:

```sh
cmake --preset clang -DCHALK_RENDERER=SDL3   # default
cmake --preset clang -DCHALK_RENDERER=DX11   # not yet implemented, fails to build on purpose
```

`engine.renderer` resolves `engine::Renderer` to the selected backend's concrete type at compile time, so every call site pays zero runtime dispatch cost. Adding Vulkan or DX12 later means writing a new backend module and one new branch in `renderer.ixx`, not touching the contract.

See [docs/adr/](adr/) for the decisions behind this shape and [docs/design/](design/) for the fuller design writeups.

## Errors

The engine does not throw. Fallible operations return their result as data: `App::resource<T>()` returns an `Option<T>` handle, a non-owning reference, empty if the resource is missing, otherwise usable with `->` and `*` at call sites (no raw pointers, no `.get()`). Systems that hit an error report it into the `Diagnostics` resource and skip that frame's work. Renderer misuse (wrong call order, a stale or destroyed handle) is reported the same way: never a crash, never undefined behavior.

Exceptions are disabled at compile time (`-fno-exceptions`, `/EHs-c-` on MSVC), so the no-throw contract is a hard guarantee rather than a convention: nothing in the engine, the standard library it pulls in, or game code can throw, and nothing can escape `main`.

## Adding a subsystem

1. Create a contract module with the shared types, e.g. `engine/plugin/camera/camera.ixx` -> `module engine.camera;`.
2. Create a backend module with a `<Subsystem><Backend>Plugin`, e.g. `engine/plugin/camera/basic/camera.basic.ixx` -> `module engine.camera.basic;`.
3. Import the backend and add the plugin in `source/main.cxx` (or `plugin.default.ixx` for the default set).

Standard headers are included directly in each module's global module fragment; `import` is reserved for engine modules.
