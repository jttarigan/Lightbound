# Slang shader compilation. One .slang source per pass; every source is compiled to
# SPIR-V on all platforms (backend parity, R1) and additionally to a .metallib on macOS.
#
#   lb_compile_shaders(<target> OUTPUT_DIR <dir> SOURCES a.slang b.slang ... SHARED shared/x.slang ...)
#
# Emits <dir>/<name>.spv and, on Apple, <dir>/<name>.metallib and <dir>/<name>.metal.

set(LB_SLANG_SPIRV_PROFILE "spirv_1_5")
set(LB_SLANG_COMMON_FLAGS
  -fvk-use-entrypoint-name
  -matrix-layout-row-major
  -warnings-as-errors all
  -O2)

function(lb_compile_shaders target)
  cmake_parse_arguments(ARG "" "OUTPUT_DIR" "SOURCES;SHARED;INCLUDE_DIRS" ${ARGN})
  set(_outputs)
  set(_incs)
  foreach(dir IN LISTS ARG_INCLUDE_DIRS)
    list(APPEND _incs -I "${dir}")
  endforeach()
  foreach(src IN LISTS ARG_SOURCES)
    get_filename_component(name "${src}" NAME_WE)
    get_filename_component(abs "${src}" ABSOLUTE)
    set(spv "${ARG_OUTPUT_DIR}/${name}.spv")
    add_custom_command(
      OUTPUT "${spv}"
      COMMAND "${LB_SLANGC}" "${abs}" ${_incs} -target spirv -profile ${LB_SLANG_SPIRV_PROFILE}
              ${LB_SLANG_COMMON_FLAGS} -o "${spv}"
      DEPENDS "${abs}" ${ARG_SHARED}
      COMMENT "slangc → SPIR-V: ${name}"
      VERBATIM)
    list(APPEND _outputs "${spv}")
    if(APPLE)
      set(mtllib "${ARG_OUTPUT_DIR}/${name}.metallib")
      set(mtlsrc "${ARG_OUTPUT_DIR}/${name}.metal")
      add_custom_command(
        OUTPUT "${mtllib}" "${mtlsrc}"
        COMMAND "${LB_SLANGC}" "${abs}" ${_incs} -target metallib ${LB_SLANG_COMMON_FLAGS} -o "${mtllib}"
        COMMAND "${LB_SLANGC}" "${abs}" ${_incs} -target metal ${LB_SLANG_COMMON_FLAGS} -o "${mtlsrc}"
        DEPENDS "${abs}" ${ARG_SHARED}
        COMMENT "slangc → metallib: ${name}"
        VERBATIM)
      list(APPEND _outputs "${mtllib}" "${mtlsrc}")
    endif()
  endforeach()
  add_custom_target(${target} DEPENDS ${_outputs})
endfunction()

# Layout generation: reflect the shared structs for every target we compile for and
# generate a header with sizes/offsets. The generator FAILS if the targets disagree.
#
#   lb_generate_layouts(<target> PROBE <probe.slang> OUTPUT <layouts.gen.h> SHARED ... INCLUDE_DIRS ...)
function(lb_generate_layouts target)
  cmake_parse_arguments(ARG "" "PROBE;OUTPUT" "SHARED;INCLUDE_DIRS" ${ARGN})
  get_filename_component(probe_abs "${ARG_PROBE}" ABSOLUTE)
  get_filename_component(out_dir "${ARG_OUTPUT}" DIRECTORY)
  set(_incs)
  foreach(dir IN LISTS ARG_INCLUDE_DIRS)
    list(APPEND _incs -I "${dir}")
  endforeach()
  set(json_spv "${out_dir}/layouts_spirv.json")
  set(jsons "${json_spv}")
  set(cmds
    COMMAND "${LB_SLANGC}" "${probe_abs}" ${_incs} -target spirv -profile ${LB_SLANG_SPIRV_PROFILE}
            -reflection-json "${json_spv}" -o "${out_dir}/layout_probe_spirv.spv")
  if(APPLE)
    set(json_mtl "${out_dir}/layouts_metal.json")
    list(APPEND jsons "${json_mtl}")
    list(APPEND cmds
      COMMAND "${LB_SLANGC}" "${probe_abs}" ${_incs} -target metal
              -reflection-json "${json_mtl}" -o "${out_dir}/layout_probe_metal.metal")
  endif()
  add_custom_command(
    OUTPUT "${ARG_OUTPUT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
    ${cmds}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/gen_layouts.py" -o "${ARG_OUTPUT}" ${jsons}
    DEPENDS "${probe_abs}" ${ARG_SHARED} "${CMAKE_SOURCE_DIR}/tools/gen_layouts.py"
    COMMENT "Generating GPU layout header from Slang reflection"
    VERBATIM)
  add_custom_target(${target} DEPENDS "${ARG_OUTPUT}")
endfunction()
