# Compile HLSL shaders to SPIR-V with the shadercross CLI whenever they change.
# Outputs land in <build>/shaders, next to the executable. Stage comes from the
# filename suffix: *.vertex.hlsl / *.fragment.hlsl / *.compute.hlsl.

set(SHADERS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(SHADERS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS "${SHADERS_SOURCE_DIR}/*.hlsl")

set(SPIRV_OUTPUTS "")
foreach(shader IN LISTS SHADER_SOURCES)
  if(NOT shader MATCHES "\.(vertex|fragment|compute)\.hlsl$")
    message(WARNING "Skipping ${shader}: cannot derive shader stage from filename")
    continue()
  endif()
  set(stage "${CMAKE_MATCH_1}")
  get_filename_component(stem "${shader}" NAME)
  string(REGEX REPLACE "\.hlsl$" "" stem "${stem}")
  set(output "${SHADERS_OUTPUT_DIR}/${stem}.spv")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "$<TARGET_FILE:shadercross>" "${shader}" -s HLSL -d SPIRV -t "${stage}" -o "${output}"
    DEPENDS "${shader}" shadercross
    COMMENT "Compiling ${stem}.hlsl -> ${stem}.spv"
    VERBATIM
  )
  list(APPEND SPIRV_OUTPUTS "${output}")
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# shadercross loads dxcompiler.dll/dxil.dll from its own directory at runtime.
if(TARGET DirectXShaderCompiler::dxcompiler)
  add_custom_command(TARGET shadercross POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:DirectXShaderCompiler::dxcompiler>"
      "$<TARGET_FILE_DIR:shadercross>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:DirectXShaderCompiler::dxil>"
      "$<TARGET_FILE_DIR:shadercross>"
    VERBATIM
  )
endif()
