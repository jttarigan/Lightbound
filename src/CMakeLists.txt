# ------------------------------------------------------------------ build info (git commit, refreshed every build)
add_custom_target(lb_gitinfo
  COMMAND "${CMAKE_COMMAND}" -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${LB_GEN_DIR}/GitInfo.h
          -P "${CMAKE_SOURCE_DIR}/cmake/LbGitInfo.cmake"
  BYPRODUCTS "${LB_GEN_DIR}/GitInfo.h"
  COMMENT "Updating GitInfo.h"
  VERBATIM)

# ------------------------------------------------------------------ core
add_library(lb_core STATIC
  core/Log.cpp
  core/JobSystem.cpp)
target_include_directories(lb_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lb_core PUBLIC Threads::Threads)
lb_set_warnings(lb_core)

# ------------------------------------------------------------------ gfx
set(LB_GFX_SOURCES gfx/Gfx.cpp)
if(LB_BACKEND STREQUAL "metal")
  list(APPEND LB_GFX_SOURCES gfx/metal/MetalGfx.cpp)
else()
  list(APPEND LB_GFX_SOURCES gfx/vulkan/VulkanGfx.cpp gfx/vulkan/vma_impl.cpp)
endif()
add_library(lb_gfx STATIC ${LB_GFX_SOURCES})
target_include_directories(lb_gfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${LB_GEN_DIR}")
target_link_libraries(lb_gfx PUBLIC lb_core SDL3::SDL3)
add_dependencies(lb_gfx lb_layouts_gen lb_shaders)
if(LB_BACKEND STREQUAL "metal")
  target_compile_definitions(lb_gfx PUBLIC LB_BACKEND_METAL=1)
  target_link_libraries(lb_gfx PUBLIC lb_metalcpp)
else()
  target_compile_definitions(lb_gfx PUBLIC LB_BACKEND_VULKAN=1)
  target_link_libraries(lb_gfx PUBLIC lb_vulkan_headers lb_vma lb_vulkan_loader)
endif()
lb_set_warnings(lb_gfx)

# ------------------------------------------------------------------ frame (strategy orchestration; M1: memory policy)
add_library(lb_frame STATIC frame/MemoryPolicy.cpp)
target_link_libraries(lb_frame PUBLIC lb_gfx)
lb_set_warnings(lb_frame)

# ------------------------------------------------------------------ bench (microbench, sysinfo, CSV writers)
add_library(lb_bench STATIC bench/Microbench.cpp bench/SysInfo.cpp)
target_include_directories(lb_bench PRIVATE "${LB_GEN_DIR}")
target_link_libraries(lb_bench PUBLIC lb_frame)
add_dependencies(lb_bench lb_gitinfo)
if(APPLE)
  target_link_libraries(lb_bench PRIVATE "-framework IOKit" "-framework CoreFoundation" "-framework Foundation")
elseif(WIN32)
  target_link_libraries(lb_bench PRIVATE PowrProf)
endif()
lb_set_warnings(lb_bench)

# ------------------------------------------------------------------ app library (CLI etc., testable)
add_library(lb_app STATIC app/Cli.cpp)
target_include_directories(lb_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lb_app PUBLIC lb_core)
lb_set_warnings(lb_app)

# ------------------------------------------------------------------ executable
add_executable(lightbound app/main.cpp)
target_link_libraries(lightbound PRIVATE lb_app lb_bench)  # lb_frame, lb_gfx, lb_core, SDL3 come transitively
set_target_properties(lightbound PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
lb_set_warnings(lightbound)
