cmake_minimum_required(VERSION 3.25)
project(lightbound VERSION 0.0.1 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(APPLE)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "" FORCE)
  endif()
  set(_lb_default_backend metal)
else()
  set(_lb_default_backend vulkan)
endif()

set(LB_BACKEND "${_lb_default_backend}" CACHE STRING "Graphics backend: metal | vulkan")
set_property(CACHE LB_BACKEND PROPERTY STRINGS metal vulkan)
if(NOT LB_BACKEND MATCHES "^(metal|vulkan)$")
  message(FATAL_ERROR "LB_BACKEND must be 'metal' or 'vulkan' (got '${LB_BACKEND}')")
endif()
if(LB_BACKEND STREQUAL "metal" AND NOT APPLE)
  message(FATAL_ERROR "LB_BACKEND=metal requires macOS")
endif()

option(LB_BUILD_TESTS "Build unit tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(LbWarnings)
include(LbDependencies)
include(LbSlang)

message(STATUS "Lightbound: backend=${LB_BACKEND} build=${CMAKE_BUILD_TYPE} slangc=${LB_SLANGC}")

add_subdirectory(shaders)
add_subdirectory(src)
if(LB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
