# ============================================================================
# tools/ibl_precompute/CMakeLists.txt - IBL Precomputation Tool
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# IBL Precompute Tool Executable
add_executable(ibl_precompute
    main.cpp
    IBLPrecompute.cpp
    IBLPrecompute.h
    DDSWriter.cpp
    DDSWriter.h
)

# Link dependencies
target_link_libraries(ibl_precompute
    PRIVATE
        metagfx_core
        metagfx_utils
)

# Include directories
target_include_directories(ibl_precompute
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/glm
        ${CMAKE_SOURCE_DIR}/external/stb
)

# Set output directory
set_target_properties(ibl_precompute PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tools
)

message(STATUS "Added IBL Precompute Tool")
