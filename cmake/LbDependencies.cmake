# All third-party code is fetched with FetchContent, pinned to tags/releases.
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
find_package(Threads REQUIRED)
find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)

# ---------------------------------------------------------------- SDL3 (static)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.4.16
  GIT_SHALLOW    TRUE
  SYSTEM)

# ---------------------------------------------------------------- doctest
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.5.3
  GIT_SHALLOW    TRUE
  SYSTEM)

# ---------------------------------------------------------------- miniaudio (header only; skip its CMake)
FetchContent_Declare(miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG        0.11.25
  GIT_SHALLOW    TRUE
  SOURCE_SUBDIR  cmake_not_used)

# ---------------------------------------------------------------- Slang binary release
set(LB_SLANG_VERSION "2026.18.2")
if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_lb_slang_asset "slang-${LB_SLANG_VERSION}-macos-aarch64.zip")
  else()
    set(_lb_slang_asset "slang-${LB_SLANG_VERSION}-macos-x86_64.zip")
  endif()
elseif(WIN32)
  set(_lb_slang_asset "slang-${LB_SLANG_VERSION}-windows-x86_64.zip")
else()
  set(_lb_slang_asset "slang-${LB_SLANG_VERSION}-linux-x86_64.zip")
endif()
FetchContent_Declare(slang_release
  URL "https://github.com/shader-slang/slang/releases/download/v${LB_SLANG_VERSION}/${_lb_slang_asset}"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(_lb_fetch_list SDL3 doctest miniaudio slang_release)

# ---------------------------------------------------------------- Metal: metal-cpp
if(LB_BACKEND STREQUAL "metal")
  # Apple publishes metal-cpp per SDK pair; macOS15_iOS18 is the newest archive
  # available and builds fine against the macOS 26 SDK (see docs/DECISIONS.md).
  FetchContent_Declare(metalcpp
    URL "https://developer.apple.com/metal/cpp/files/metal-cpp_macOS15_iOS18.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  list(APPEND _lb_fetch_list metalcpp)
endif()

# ---------------------------------------------------------------- Vulkan: headers + VMA (+ MoltenVK on macOS)
if(LB_BACKEND STREQUAL "vulkan")
  # Tag tarballs rather than git: a shallow git clone of Vulkan-Headers fetches every tag
  # tip (--no-single-branch) and takes tens of minutes.
  FetchContent_Declare(vulkan_headers
    URL      "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz"
    URL_HASH SHA256=e87dce08116151f6b6d7de6b6faf41498e87e6cf848ff16fa3bd5402190ad4a3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR  cmake_not_used)
  FetchContent_Declare(vma
    URL      "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.4.0.tar.gz"
    URL_HASH SHA256=822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR  cmake_not_used)
  list(APPEND _lb_fetch_list vulkan_headers vma)
  if(APPLE)
    FetchContent_Declare(moltenvk
      URL "https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.2/MoltenVK-macos.tar"
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    list(APPEND _lb_fetch_list moltenvk)
  endif()
endif()

FetchContent_MakeAvailable(${_lb_fetch_list})

# ---------------------------------------------------------------- interface targets
add_library(lb_miniaudio INTERFACE)
target_include_directories(lb_miniaudio SYSTEM INTERFACE "${miniaudio_SOURCE_DIR}")

# slangc executable
if(EXISTS "${slang_release_SOURCE_DIR}/bin/slangc${CMAKE_EXECUTABLE_SUFFIX}")
  set(_lb_slangc_default "${slang_release_SOURCE_DIR}/bin/slangc${CMAKE_EXECUTABLE_SUFFIX}")
else()
  file(GLOB _lb_slangc_glob "${slang_release_SOURCE_DIR}/*/bin/slangc${CMAKE_EXECUTABLE_SUFFIX}")
  list(GET _lb_slangc_glob 0 _lb_slangc_default)
endif()
set(LB_SLANGC "${_lb_slangc_default}" CACHE FILEPATH "Path to slangc")
if(NOT EXISTS "${LB_SLANGC}")
  message(FATAL_ERROR "slangc not found at '${LB_SLANGC}'")
endif()

if(LB_BACKEND STREQUAL "metal")
  if(EXISTS "${metalcpp_SOURCE_DIR}/metal-cpp/Metal/Metal.hpp")
    set(_lb_metalcpp_dir "${metalcpp_SOURCE_DIR}/metal-cpp")
  else()
    set(_lb_metalcpp_dir "${metalcpp_SOURCE_DIR}")
  endif()
  add_library(lb_metalcpp INTERFACE)
  target_include_directories(lb_metalcpp SYSTEM INTERFACE "${_lb_metalcpp_dir}")
  target_link_libraries(lb_metalcpp INTERFACE
    "-framework Metal" "-framework QuartzCore" "-framework Foundation" "-framework CoreGraphics")
endif()

if(LB_BACKEND STREQUAL "vulkan")
  add_library(lb_vulkan_headers INTERFACE)
  target_include_directories(lb_vulkan_headers SYSTEM INTERFACE "${vulkan_headers_SOURCE_DIR}/include")
  add_library(lb_vma INTERFACE)
  target_include_directories(lb_vma SYSTEM INTERFACE "${vma_SOURCE_DIR}/include")

  # Loader: Vulkan SDK if present; otherwise on macOS link MoltenVK directly (optional control condition).
  add_library(lb_vulkan_loader INTERFACE)
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    target_link_libraries(lb_vulkan_loader INTERFACE ${Vulkan_LIBRARIES})
    message(STATUS "Lightbound: Vulkan loader from SDK: ${Vulkan_LIBRARIES}")
  elseif(APPLE)
    if(EXISTS "${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib")
      set(_lb_mvk_dylib "${moltenvk_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib")
    else()
      set(_lb_mvk_dylib "${moltenvk_SOURCE_DIR}/dynamic/dylib/macOS/libMoltenVK.dylib")
    endif()
    if(NOT EXISTS "${_lb_mvk_dylib}")
      message(FATAL_ERROR "MoltenVK dylib not found under ${moltenvk_SOURCE_DIR}")
    endif()
    target_link_libraries(lb_vulkan_loader INTERFACE "${_lb_mvk_dylib}")
    target_compile_definitions(lb_vulkan_loader INTERFACE LB_VULKAN_LIBRARY_PATH="${_lb_mvk_dylib}" LB_VULKAN_MOLTENVK=1)
    message(STATUS "Lightbound: Vulkan via MoltenVK: ${_lb_mvk_dylib}")
  else()
    message(FATAL_ERROR "Vulkan SDK not found (install LunarG Vulkan SDK and set VULKAN_SDK)")
  endif()
endif()
