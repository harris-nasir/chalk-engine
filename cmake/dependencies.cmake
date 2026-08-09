# Fetch third-party dependencies via CPM.
# Included once from the main CMakeLists.txt before any plugin backends are
# configured, so backend CMakeLists can use the imported targets directly.

CPMFindPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.14
  GIT_SHALLOW TRUE
  OPTIONS "SDL_SHARED ON" "SDL_STATIC OFF"
  SYSTEM TRUE
)

CPMAddPackage(
  NAME SPIRV-Cross
  GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
  GIT_TAG main
  GIT_SHALLOW TRUE
  OPTIONS
    "SPIRV_CROSS_STATIC ON"
    "SPIRV_CROSS_SHARED OFF"
    "SPIRV_CROSS_CLI OFF"
    "SPIRV_CROSS_ENABLE_TESTS OFF"
  SYSTEM TRUE
)
# SDL_shadercross guards its spirv-cross dependency behind an underscore-named
# target check; alias the hyphenated target it actually links against.
add_library(spirv_cross_c ALIAS spirv-cross-c)

CPMFindPackage(
  NAME SDL3_shadercross
  GITHUB_REPOSITORY libsdl-org/SDL_shadercross
  GIT_TAG e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba
  GIT_SHALLOW TRUE
  OPTIONS
    "SDLSHADERCROSS_VENDORED OFF"
    "SDLSHADERCROSS_DXC ON"
    "SDLSHADERCROSS_SPIRVCROSS_SHARED OFF"
    "SDLSHADERCROSS_SHARED OFF"
    "SDLSHADERCROSS_STATIC ON"
    "SDLSHADERCROSS_CLI ON"
    "SDLSHADERCROSS_INSTALL OFF"
    "SDLSHADERCROSS_TESTS OFF"
  SYSTEM TRUE
)
