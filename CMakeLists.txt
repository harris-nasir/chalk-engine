cmake_minimum_required(VERSION 4.4)
project(chalk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/external" CACHE PATH "CPM dependency source cache")

include(cmake/CompilerWarnings.cmake)
include(cmake/get_cpm.cmake)
include(cmake/plugins.cmake)
include(cmake/dependencies.cmake)
include(cmake/shaders.cmake)

# ---------------------------------------------------------------------------
# Plugin backend options
# ---------------------------------------------------------------------------
set(CHALK_PLATFORM "SDL3" CACHE STRING "Platform backend")
set_property(CACHE CHALK_PLATFORM PROPERTY STRINGS "SDL3" "WIN32")

set(CHALK_RENDERER "SDL3" CACHE STRING "Renderer backend")
set_property(CACHE CHALK_RENDERER PROPERTY STRINGS "SDL3" "DX11")

set(CHALK_AUDIO "None" CACHE STRING "Audio backend")
set_property(CACHE CHALK_AUDIO PROPERTY STRINGS "None" "XAudio2")

set(CHALK_INPUT "None" CACHE STRING "Input backend")
set_property(CACHE CHALK_INPUT PROPERTY STRINGS "None" "SDL3")

set(CHALK_PHYSICS "None" CACHE STRING "Physics backend")
set_property(CACHE CHALK_PHYSICS PROPERTY STRINGS "None" "Simple")

if(NOT CHALK_PLATFORM MATCHES "^(SDL3|WIN32)$")
  message(FATAL_ERROR "CHALK_PLATFORM must be SDL3 or WIN32 (got '${CHALK_PLATFORM}')")
endif()
if(NOT CHALK_RENDERER MATCHES "^(SDL3|DX11)$")
  message(FATAL_ERROR "CHALK_RENDERER must be SDL3 or DX11 (got '${CHALK_RENDERER}')")
endif()
if(NOT CHALK_AUDIO MATCHES "^(None|XAudio2)$")
  message(FATAL_ERROR "CHALK_AUDIO must be None or XAudio2 (got '${CHALK_AUDIO}')")
endif()
if(NOT CHALK_INPUT MATCHES "^(None|SDL3)$")
  message(FATAL_ERROR "CHALK_INPUT must be None or SDL3 (got '${CHALK_INPUT}')")
endif()
if(NOT CHALK_PHYSICS MATCHES "^(None|Simple)$")
  message(FATAL_ERROR "CHALK_PHYSICS must be None or Simple (got '${CHALK_PHYSICS}')")
endif()

# ---------------------------------------------------------------------------
# Core static library
# ---------------------------------------------------------------------------
add_library(chalk_core STATIC)
target_sources(chalk_core
  PUBLIC
  FILE_SET CXX_MODULES
  FILES
    engine/core/core.ixx
    engine/core/core.option.ixx
    engine/core/core.type.ixx
    engine/plugin/platform/platform.ixx
    engine/plugin/renderer/renderer.types.ixx
    engine/plugin/renderer/renderer.handle_table.ixx
    engine/plugin/renderer/renderer.token_guard.ixx
    engine/plugin/audio/audio.ixx
    engine/plugin/input/input.ixx
    engine/plugin/physics/physics.ixx
    engine/plugin/scene/scene.ixx
    engine/plugin/math/math.ixx
)
set_project_warnings(chalk_core)
set_chalk_exceptions(chalk_core)

# ---------------------------------------------------------------------------
# Backend static libraries (only selected ones are compiled/linked)
# ---------------------------------------------------------------------------
set(CHALK_PLUGIN_LIBS "")

if(CHALK_PLATFORM STREQUAL "SDL3")
  include(engine/plugin/platform/sdl3/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_platform_sdl3)
elseif(CHALK_PLATFORM STREQUAL "WIN32")
  include(engine/plugin/platform/win32/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_platform_win32)
endif()

if(CHALK_RENDERER STREQUAL "SDL3")
  include(engine/plugin/renderer/sdl3/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_renderer_sdl3)
elseif(CHALK_RENDERER STREQUAL "DX11")
  include(engine/plugin/renderer/dx11/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_renderer_dx11)
endif()

if(CHALK_AUDIO STREQUAL "XAudio2")
  include(engine/plugin/audio/xaudio2/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_audio_xaudio2)
endif()

if(CHALK_INPUT STREQUAL "SDL3")
  include(engine/plugin/input/sdl3/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_input_sdl3)
endif()

if(CHALK_PHYSICS STREQUAL "Simple")
  include(engine/plugin/physics/simple/CMakeLists.txt)
  list(APPEND CHALK_PLUGIN_LIBS chalk_physics_simple)
endif()

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
  PRIVATE
  source/main.cxx

  PRIVATE
  FILE_SET CXX_MODULES
  FILES
    engine/engine.ixx
    engine/plugin/renderer/renderer.ixx
    engine/plugin/plugin.default.ixx
    source/camera.ixx
)

add_dependencies(${PROJECT_NAME} shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
  VERBATIM
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  CHALK_PLATFORM_${CHALK_PLATFORM}
  CHALK_RENDERER_${CHALK_RENDERER}
  CHALK_AUDIO_${CHALK_AUDIO}
  CHALK_INPUT_${CHALK_INPUT}
  CHALK_PHYSICS_${CHALK_PHYSICS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE chalk_core ${CHALK_PLUGIN_LIBS})

set_project_warnings(${PROJECT_NAME})
set_chalk_exceptions(${PROJECT_NAME})
