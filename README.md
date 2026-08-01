# Build Instructions

Build with CMake presets (choose your compiler):

``` console
cmake --preset clang
cmake --build --preset clang
```

Or without presets:

``` console
cmake -S . -B build
cmake --build build
```

Available presets: `clang`, `gcc`
