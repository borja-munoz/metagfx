# ============================================================================
# Root CMakeLists.txt
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(metagfx VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(METAGFX_BUILD_TESTS "Build tests" OFF)
option(METAGFX_USE_VULKAN "Enable Vulkan support" ON)
option(METAGFX_USE_D3D12 "Enable Direct3D 12 support" OFF)
option(METAGFX_USE_METAL "Enable Metal support" OFF)
option(METAGFX_USE_WEBGPU "Enable WebGPU support" OFF)

# Platform detection
if(WIN32)
    set(METAGFX_PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(METAGFX_PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(METAGFX_PLATFORM_LINUX TRUE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Find Vulkan (needed for ImGui backends)
if(METAGFX_USE_VULKAN)
    find_package(Vulkan REQUIRED)
endif()

# External dependencies
add_subdirectory(external)

# Project modules
add_subdirectory(src/core)
add_subdirectory(src/rhi)
add_subdirectory(src/utils)
add_subdirectory(src/scene)
add_subdirectory(src/renderer)
add_subdirectory(src/app)

# Tools
add_subdirectory(tools/ibl_precompute)

# Tests
if(METAGFX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
