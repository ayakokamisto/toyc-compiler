# cmake/CompilerWarnings.cmake
# Sets reasonable warnings for GCC, Clang, and MSVC.
# TOYC_WARNINGS_AS_ERRORS=ON enables -Werror / /WX.

function(toyc_set_warnings target)
  if(TOYC_WARNINGS_AS_ERRORS)
    set(TOYC_WERROR ON)
  else()
    set(TOYC_WERROR OFF)
  endif()

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(TOYC_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wno-unused-parameter
    )
    if(TOYC_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
