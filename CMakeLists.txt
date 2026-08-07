cmake_minimum_required(VERSION 4.4)
project(chalk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CHALK_RENDERER "SDL3" CACHE STRING "Renderer backend selected at compile time")
set_property(CACHE CHALK_RENDERER PROPERTY STRINGS SDL3 DX11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/external" CACHE PATH "CPM dependency source cache")

include(cmake/CompilerWarnings.cmake)
include(cmake/get_cpm.cmake)

CPMFindPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.4.14
  OPTIONS "SDL_SHARED ON" "SDL_STATIC OFF"
  SYSTEM TRUE
)

CPMFindPackage(
  NAME SDL3_image
  GITHUB_REPOSITORY libsdl-org/SDL_image
  GIT_TAG release-3.4.4
  SYSTEM TRUE
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "source/*.cxx")
file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "engine/*.ixx" "source/*.ixx")

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
  PRIVATE
  ${SOURCES}

  PRIVATE
  FILE_SET CXX_MODULES
  FILES ${MODULE_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 SDL3_image::SDL3_image d3d11)

set_project_warnings(${PROJECT_NAME})

if(CHALK_RENDERER STREQUAL "SDL3")
  target_compile_definitions(${PROJECT_NAME} PRIVATE CHALK_RENDERER_SDL3)
elseif(CHALK_RENDERER STREQUAL "DX11")
  target_compile_definitions(${PROJECT_NAME} PRIVATE CHALK_RENDERER_DX11)
else()
  message(FATAL_ERROR "Unknown CHALK_RENDERER value: ${CHALK_RENDERER} (expected SDL3 or DX11)")
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /EHs-c-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions)
endif()
