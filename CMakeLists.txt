cmake_minimum_required(VERSION 4.4)
project(chalk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)

set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/external" CACHE PATH "CPM dependency source cache")

include(cmake/get_cpm.cmake)

CPMFindPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG release-3.2.0
  OPTIONS "SDL_SHARED ON" "SDL_STATIC OFF"
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

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

set_project_warnings(${PROJECT_NAME})

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /EHs-c-)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -fno-exceptions)
endif()
