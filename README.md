# chalk

A small, modular game engine written in C++26 modules. An `App` owns typed resources, runs systems in a fixed per-frame pipeline, and plugins register both from the outside; the core knows nothing about any specific subsystem.

## Status

Early, actively developed. Windows only for now (SDL3 for platform/windowing, SDL3 GPU for rendering, XInput for gamepad, XAudio2 for audio). The renderer is a real primitive contract with one working backend today (SDL3 GPU); Vulkan and DX12 are planned as additional backends, not contract changes.

## Build

```sh
cmake --preset clang        # or: gcc
cmake --build --preset clang
./build/chalk.exe
```

Requires a C++26 compiler with C++20 module support (Clang >= 18, GCC >= 15). Dependencies (SDL3, SDL3_image) are fetched automatically via CPM on first configure.

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): module layout, core concepts, the renderer contract, how to add a subsystem
- [docs/adr/](docs/adr/): architecture decision records
- [docs/design/](docs/design/): design writeups for individual subsystems
