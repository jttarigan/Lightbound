# Warnings-as-errors for project code only. Third-party code is included as SYSTEM.
function(lb_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4 /WX /permissive- /Zc:__cplusplus /Zc:preprocessor /utf-8 /EHsc
      /wd4324   # structure was padded due to alignment specifier (intentional for GPU structs)
    )
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
      -Wnon-virtual-dtor -Wcast-align -Wunused
      -Wnull-dereference -Wformat=2
      -Werror
    )
  endif()
endfunction()
