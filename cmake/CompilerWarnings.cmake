function(set_project_warnings TARGET_NAME)
  option(CHALK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

  if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
            /W4
            /permissive-
            /utf-8
        )

    if(CHALK_WARNINGS_AS_ERRORS)
      target_compile_options(${TARGET_NAME} PRIVATE /WX)
    endif()

  else()
    target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
            -Wmisleading-indentation
            -Wno-unknown-pragmas
        )

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${TARGET_NAME} PRIVATE
                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            )
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|IntelLLVM")
      target_compile_options(${TARGET_NAME} PRIVATE
                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic
            )
    endif()

    if(CHALK_WARNINGS_AS_ERRORS)
      target_compile_options(${TARGET_NAME} PRIVATE -Werror)
    endif()
  endif()
endfunction()
