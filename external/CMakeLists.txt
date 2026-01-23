# ============================================================================
# external/CMakeLists.txt
# ============================================================================
# SDL3
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# GLM (header-only)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/glm)

# stb (header-only)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/stb)

# Assimp - 3D model loading
# Disable unnecessary Assimp features to speed up compilation
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

# Enable only the formats we need (we can add more as needed)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "" FORCE)

add_subdirectory(assimp)

# Organize Assimp in IDE folder
set_target_properties(assimp PROPERTIES FOLDER "External")
if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "External")
endif()

# Dear ImGui - immediate-mode GUI library
# Build ImGui as static libraries (core + backends)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# ImGui core library
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
set_target_properties(imgui PROPERTIES FOLDER "External")

# ImGui backends library (SDL3 + Vulkan)
set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)

# Add Vulkan backend if enabled
if(METAGFX_USE_VULKAN)
    list(APPEND IMGUI_BACKEND_SOURCES ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
endif()

# Add Metal backend if enabled
if(METAGFX_USE_METAL)
    list(APPEND IMGUI_BACKEND_SOURCES ${IMGUI_DIR}/backends/imgui_impl_metal.mm)
endif()

# Add WebGPU backend if enabled
if(METAGFX_USE_WEBGPU)
    list(APPEND IMGUI_BACKEND_SOURCES ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp)
endif()

add_library(imgui_backends STATIC ${IMGUI_BACKEND_SOURCES})
target_include_directories(imgui_backends
    PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)
target_link_libraries(imgui_backends
    PUBLIC
        imgui
        SDL3::SDL3
)

# Link Vulkan for ImGui if enabled
if(METAGFX_USE_VULKAN)
    target_link_libraries(imgui_backends PUBLIC Vulkan::Vulkan)
endif()

# Link Metal frameworks for ImGui if enabled
if(METAGFX_USE_METAL)
    find_library(METAL_FRAMEWORK Metal)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    target_link_libraries(imgui_backends PUBLIC ${METAL_FRAMEWORK} ${QUARTZCORE_FRAMEWORK})
    # Enable Metal C++ bindings for ImGui and add metal-cpp include path
    target_compile_definitions(imgui_backends PUBLIC IMGUI_IMPL_METAL_CPP)
    target_include_directories(imgui_backends PRIVATE ${CMAKE_SOURCE_DIR}/external/metal-cpp)
endif()

set_target_properties(imgui_backends PROPERTIES FOLDER "External")

# SPIRV-Cross for shader translation (used by Metal and WebGPU)
if(METAGFX_USE_METAL OR METAGFX_USE_WEBGPU)
    # Configure SPIRV-Cross build options
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)   # Required for MSL and WGSL analysis
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL ${METAGFX_USE_METAL} CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(spirv-cross-core PROPERTIES FOLDER "External/SPIRV-Cross")
    set_target_properties(spirv-cross-glsl PROPERTIES FOLDER "External/SPIRV-Cross")

    if(METAGFX_USE_METAL)
        set_target_properties(spirv-cross-msl PROPERTIES FOLDER "External/SPIRV-Cross")
    endif()
endif()

# Google Dawn - WebGPU implementation (used by WebGPU backend)
# Dawn is built externally to avoid massive dependency bloat in the project
if(METAGFX_USE_WEBGPU)
    message(STATUS "Looking for external Dawn installation...")

    # Option to specify custom Dawn installation path
    set(DAWN_DIR "" CACHE PATH "Path to Dawn installation directory")

    if(DAWN_DIR)
        # User provided explicit path
        set(CMAKE_PREFIX_PATH ${DAWN_DIR} ${CMAKE_PREFIX_PATH})
        message(STATUS "  Using Dawn from: ${DAWN_DIR}")
    endif()

    # Try to find Dawn package
    find_package(dawn QUIET CONFIG)

    if(NOT dawn_FOUND)
        message(STATUS "")
        message(STATUS "==============================================================")
        message(STATUS "Dawn not found. You need to build Dawn externally.")
        message(STATUS "")
        message(STATUS "Instructions:")
        message(STATUS "  1. Clone Dawn outside your project:")
        message(STATUS "     cd ~/dev  # Or any directory outside MetaGFX")
        message(STATUS "     git clone https://dawn.googlesource.com/dawn")
        message(STATUS "     cd dawn")
        message(STATUS "     git checkout main  # Use main branch for latest Tint API")
        message(STATUS "")
        message(STATUS "  2. Build and install Dawn:")
        message(STATUS "     mkdir build && cd build")
        message(STATUS "     cmake .. -DCMAKE_BUILD_TYPE=Release \\")
        message(STATUS "              -DCMAKE_CXX_STANDARD=20 \\")
        message(STATUS "              -DCMAKE_INSTALL_PREFIX=~/dawn-install \\")
        message(STATUS "              -DDAWN_FETCH_DEPENDENCIES=ON \\")
        message(STATUS "              -DDAWN_BUILD_SAMPLES=OFF \\")
        message(STATUS "              -DDAWN_ENABLE_INSTALL=ON \\")
        message(STATUS "              -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC \\")
        message(STATUS "              -DTINT_BUILD_SPV_READER=ON \\")
        message(STATUS "              -DTINT_BUILD_WGSL_WRITER=ON")
        message(STATUS "     make -j$(sysctl -n hw.ncpu)")
        message(STATUS "     make install")
        message(STATUS "")
        message(STATUS "  3. Configure MetaGFX with Dawn path:")
        message(STATUS "     cmake .. -DMETAGFX_USE_WEBGPU=ON \\")
        message(STATUS "              -DDAWN_DIR=~/dawn-install")
        message(STATUS "==============================================================")
        message(STATUS "")
        message(FATAL_ERROR "Dawn is required for WebGPU backend but was not found.")
    endif()

    message(STATUS "  Found Dawn at: ${dawn_DIR}")
    message(STATUS "  Dawn version: ${dawn_VERSION}")

    # Add Dawn include directories to ImGui WebGPU backend
    target_include_directories(imgui_backends
        PRIVATE
            /Users/Borja/dev/dawn/dawn_install/include  # Dawn headers location
    )

    # Define IMGUI_IMPL_WEBGPU_BACKEND_DAWN for ImGui
    target_compile_definitions(imgui_backends
        PRIVATE
            IMGUI_IMPL_WEBGPU_BACKEND_DAWN=1
    )

    # Link ImGui to Dawn
    target_link_libraries(imgui_backends PUBLIC webgpu_dawn)

    if(TARGET dawn_platform)
        set_target_properties(dawn_platform PROPERTIES FOLDER "External/Dawn")
    endif()
    if(TARGET dawn_common)
        set_target_properties(dawn_common PROPERTIES FOLDER "External/Dawn")
    endif()
    if(TARGET libtint)
        set_target_properties(libtint PROPERTIES FOLDER "External/Dawn/Tint")
    endif()

    # Link Dawn to ImGui WebGPU backend
    target_link_libraries(imgui_backends PUBLIC webgpu_dawn)
    target_include_directories(imgui_backends PRIVATE ${dawn_SOURCE_DIR}/include)
endif()