# chalk

A small, modular game engine written in C++26 modules. The core is a plugin host: an `App` owns typed **resources**, runs **systems** in a fixed per-frame pipeline, and **plugins** register resources and systems. Subsystems (platform, renderer, input, audio, physics) are separate modules that plug in from the outside; the core knows nothing about them.

## Layout

```
engine/
  engine.ixx                              module engine                  : facade, re-exports every contract
  core/core.ixx                           module engine.core             : App, Schedule, resources, systems, CorePlugin
  plugin/
    platform/platform.ixx                 module engine.platform         : WindowDescription / Window
    platform/win32/platform.win32.ixx     module engine.platform.win32   : PlatformWin32Plugin
    renderer/renderer.ixx                 module engine.renderer
    renderer/dx11/renderer.dx11.ixx       module engine.renderer.dx11    : RendererDx11Plugin
    audio/audio.ixx                       module engine.audio
    audio/xaudio2/audio.xaudio2.ixx       module engine.audio.xaudio2    : AudioXAudio2Plugin
    input/input.ixx                       module engine.input            : InputState
    input/xinput/input.xinput.ixx         module engine.input.xinput     : InputXInputPlugin
    physics/physics.ixx                   module engine.physics          : PhysicsWorld
    physics/simple/physics.simple.ixx     module engine.physics.simple   : PhysicsSimplePlugin
source/
  main.cxx                                : composition root
```

Each subsystem has two modules: a **contract** with the shared types, and **backends** that implement it. `engine.ixx` re-exports core and every contract; it never imports a backend.

## Concepts

- **Resources**: one instance of any type, addressed by type: `app.insert_resource<T>({})`, `app.resource<T>()` (returns an `Option<T>` handle).
- **Systems**: plain functions `void(App&)` run each frame in pipeline order (`Startup`, `PreUpdate`, `Update`, `PostUpdate`, `Render`, `Shutdown`).
- **Plugins**: any type with `void build(App&)` that registers resources and systems.

`source/main.cxx` is the composition root: it imports the concrete backend modules and adds their plugins in dependency order. Swapping a backend is editing one import line and one plugin name.

## Errors

The engine does not throw. Fallible operations return their result as data: `App::resource<T>()` returns an `Option<T>` handle, a non-owning reference, empty if the resource is missing, otherwise usable with `->` and `*` at call sites (no raw pointers, no `.get()`). Systems that hit an error report it into the `Diagnostics` resource and skip that frame's work.

Exceptions are disabled at compile time (`-fno-exceptions`, `/EHs-c-` on MSVC), so the no-throw contract is a hard guarantee rather than a convention: nothing in the engine, the standard library it pulls in, or game code can throw, and nothing can escape `main`.

## Adding a subsystem

1. Create a contract module with the shared types, e.g. `engine/plugin/camera/camera.ixx` → `module engine.camera;`.
2. Create a backend module with a `<Subsystem><Backend>Plugin`, e.g. `engine/plugin/camera/basic/camera.basic.ixx` → `module engine.camera.basic;`.
3. Import the backend and add the plugin in `source/main.cxx`.

Standard headers are included directly in each module's global module fragment; `import` is reserved for engine modules.

## Build

``` sh
cmake --preset clang        # or: gcc
cmake --build --preset clang
./build/chalk.exe
```

Requires a C++26 compiler with C++20 module support (Clang ≥ 18, GCC ≥ 15).
