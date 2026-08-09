# Architecture

## Layout

```
engine/
  engine.ixx                                module engine                  : facade, re-exports every contract
  core/core.ixx                             module engine.core             : App, Schedule, resources, systems
  plugin/
    platform/platform.ixx                   module engine.platform         : WindowDescription / Window / NativeWindowHandle
    platform/sdl3/platform.sdl3.ixx         module engine.platform.sdl3    : PlatformSDL3Plugin
    platform/win32/platform.win32.ixx       module engine.platform.win32   : PlatformWin32Plugin (used by the DX11 renderer path)
    renderer/renderer.types.ixx             module engine.renderer.types   : handles, description structs, RendererBackend concept
    renderer/renderer.ixx                   module engine.renderer         : use_sdl3_backend flag, Renderer alias, load_shader
    renderer/sdl3/renderer.sdl3.ixx         module engine.renderer.sdl3    : SDL3Renderer (implements RendererBackend), RendererSDL3Plugin
    renderer/dx11/renderer.dx11.ixx         module engine.renderer.dx11    : DX11Renderer (implements RendererBackend), RendererDX11Plugin
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

`source/main.cxx` is the composition root: it adds the default plugins, which pull in the platform and renderer backend pair selected at compile time.

## Backend selection

Every plugin backend is built as its own CMake static library:

- `chalk_platform_sdl3` / `chalk_platform_win32`
- `chalk_renderer_sdl3` / `chalk_renderer_dx11`
- `chalk_audio_xaudio2`
- `chalk_input_xinput`
- `chalk_physics_simple`

Each plugin category is selected independently through a CMake cache option:

```cmake
set(CHALK_PLATFORM "SDL3" CACHE STRING "Platform backend")
set(CHALK_RENDERER "SDL3" CACHE STRING "Renderer backend")
set(CHALK_AUDIO "None" CACHE STRING "Audio backend")
set(CHALK_INPUT "None" CACHE STRING "Input backend")
set(CHALK_PHYSICS "None" CACHE STRING "Physics backend")
```

CMake validates each option, maps it to a compile definition (e.g. `CHALK_PLATFORM_SDL3`, `CHALK_RENDERER_DX11`, `CHALK_AUDIO_XAUDIO2`, `CHALK_AUDIO_NONE`), and links only the selected backend libraries into the main `chalk` executable. Backend libraries are `EXCLUDE_FROM_ALL`, so unselected backends are not even compiled.

`engine.renderer` uses those definitions to import the selected renderer module and alias `engine::Renderer`:

```cpp
#if defined(CHALK_RENDERER_SDL3)
import engine.renderer.sdl3;
#elif defined(CHALK_RENDERER_DX11)
import engine.renderer.dx11;
#endif

// ...
using Renderer = SDL3Renderer; // or DX11Renderer
```

`engine.plugin_default` does the same for every category. Cross-combinations are allowed (for example, `CHALK_PLATFORM=WIN32` with `CHALK_RENDERER=SDL3`) as long as each backend's required resources are present.

Because each category is an independent option, you do not need one preset per permutation. Presets are useful only for saving common combinations (e.g. a default SDL3 preset or a Win32/DX11 preset).

The engine facade (`engine.engine.ixx`), the renderer alias (`engine.renderer`), and the default plugin (`engine.plugin_default`) live in the main executable target so they can see whichever backend modules the executable links.

## The renderer contract

`engine.renderer.types` defines the renderer as a primitive device surface: opaque handles (buffers, textures, shaders, pipelines), a frame/pass/draw call vocabulary, the same shape real GPU APIs share, not feature-level helpers like `draw_quad`. A backend is any class satisfying the `RendererBackend` concept: no inheritance, no virtual dispatch, checked at compile time with `static_assert`.

Adding Vulkan or DX12 later means writing a new backend module, adding its directory name to the CMake option strings, and adding one more branch to the `#if` chains in `engine/plugin/renderer/renderer.ixx` and `engine/plugin/plugin.default.ixx`.

See [docs/adr/](adr/) for the decisions behind this shape and [docs/design/](design/) for the fuller design writeups.

## Errors

The engine does not throw. Fallible operations return their result as data: `App::resource<T>()` returns an `Option<T>` handle, a non-owning reference, empty if the resource is missing, otherwise usable with `->` and `*` at call sites (no raw pointers, no `.get()`). Systems that hit an error report it into the `Diagnostics` resource and skip that frame's work. Renderer misuse (wrong call order, a stale or destroyed handle) is reported the same way: never a crash, never undefined behavior.

Exceptions are disabled at compile time (`-fno-exceptions`, `/EHs-c-` on MSVC), so the no-throw contract is a hard guarantee rather than a convention: nothing in the engine, the standard library it pulls in, or game code can throw, and nothing can escape `main`.

## Adding a subsystem

1. Create a contract module with the shared types, e.g. `engine/plugin/camera/camera.ixx` -> `module engine.camera;`.
2. Create a backend module with a `<Subsystem><Backend>Plugin`, e.g. `engine/plugin/camera/basic/camera.basic.ixx` -> `module engine.camera.basic;`.
3. Import the backend and add the plugin in `source/main.cxx` (or `plugin.default.ixx` for the default set).

Standard headers are included directly in each module's global module fragment; `import` is reserved for engine modules.
