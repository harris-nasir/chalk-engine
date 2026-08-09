# chalk

A small, modular game engine written in C++26 modules. An `App` owns typed resources, runs systems in a fixed per-frame pipeline, and plugins register both from the outside; the core knows nothing about any specific subsystem.

## Build

```sh
cmake --preset clang        # or: gcc
cmake --build --preset clang
./build/chalk.exe
```

## Requirements
Requires a C++26 compiler with C++20 module support (Clang >= 18, GCC >= 15).
