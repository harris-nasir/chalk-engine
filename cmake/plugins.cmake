# Plugin backend helpers.
#
# Usage in the main CMakeLists.txt:
#   include(cmake/plugins.cmake)
#   add_chalk_backend_plugin(<target_name> <module-files>...)
#
# The created target is a STATIC library marked EXCLUDE_FROM_ALL, linked
# PUBLICly against chalk_core, with project warnings and exceptions disabled.

function(add_chalk_backend_plugin target_name)
  # CMAKE_CURRENT_LIST_DIR is the directory of the CMakeLists.txt that called
  # this function, so included backend lists still glob their own files.
  file(GLOB _plugin_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.ixx")

  add_library(${target_name} STATIC EXCLUDE_FROM_ALL)
  target_sources(${target_name}
    PUBLIC
    FILE_SET CXX_MODULES
    FILES ${_plugin_sources}
  )
  target_link_libraries(${target_name} PUBLIC chalk_core)
  set_project_warnings(${target_name})
  set_chalk_exceptions(${target_name})
endfunction()

# Disable exceptions for a target consistently across MSVC and Clang/GCC.
function(set_chalk_exceptions target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /EHs-c-)
  else()
    target_compile_options(${target_name} PRIVATE -fno-exceptions)
  endif()
endfunction()
