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

# SPIRV-Cross for shader translation (used by Metal and WebGPU backends)
if(METAGFX_USE_METAL OR METAGFX_USE_WEBGPU)
    # Configure SPIRV-Cross build options
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)   # Required for MSL
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)

    add_subdirectory(SPIRV-Cross)
    set_target_properties(spirv-cross-core PROPERTIES FOLDER "External/SPIRV-Cross")
    set_target_properties(spirv-cross-glsl PROPERTIES FOLDER "External/SPIRV-Cross")
    set_target_properties(spirv-cross-msl PROPERTIES FOLDER "External/SPIRV-Cross")
endif()