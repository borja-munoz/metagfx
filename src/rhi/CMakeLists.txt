# ============================================================================
# src/rhi/CMakeLists.txt
# ============================================================================
set(RHI_SOURCES
    GraphicsDevice.cpp
)

set(RHI_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/GraphicsDevice.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Types.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Buffer.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Texture.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Sampler.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Shader.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Pipeline.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/CommandBuffer.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/SwapChain.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/Framebuffer.h
)

# Vulkan-specific sources
if(METAGFX_USE_VULKAN)
    list(APPEND RHI_SOURCES
        vulkan/VulkanTypes.cpp
        vulkan/VulkanDevice.cpp
        vulkan/VulkanSwapChain.cpp
        vulkan/VulkanBuffer.cpp
        vulkan/VulkanTexture.cpp
        vulkan/VulkanSampler.cpp
        vulkan/VulkanShader.cpp
        vulkan/VulkanPipeline.cpp
        vulkan/VulkanCommandBuffer.cpp
        vulkan/VulkanDescriptorSet.cpp
        vulkan/VulkanFramebuffer.cpp
    )
    list(APPEND RHI_HEADERS
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanTypes.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanDevice.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanSwapChain.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanTexture.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanSampler.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanShader.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanPipeline.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanCommandBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanDescriptorSet.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/vulkan/VulkanFramebuffer.h
    )
endif()

# Metal-specific sources (must be before add_library)
# Using metal-cpp (Apple's C++ wrapper) for most files
# MetalSDLBridge.mm is the only Obj-C++ file for SDL bridging
if(METAGFX_USE_METAL AND APPLE)
    list(APPEND RHI_SOURCES
        metal/MetalTypes.cpp
        metal/MetalDevice.cpp
        metal/MetalSwapChain.cpp
        metal/MetalSDLBridge.mm      # Only Obj-C file: SDL_Metal_GetLayer bridging
        metal/MetalBuffer.cpp
        metal/MetalTexture.cpp
        metal/MetalSampler.cpp
        metal/MetalShader.cpp
        metal/MetalPipeline.cpp
        metal/MetalCommandBuffer.cpp
        metal/MetalDescriptorSet.cpp
        metal/MetalFramebuffer.cpp
    )
    list(APPEND RHI_HEADERS
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalTypes.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalDevice.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalSwapChain.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalTexture.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalSampler.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalShader.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalPipeline.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalCommandBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalDescriptorSet.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/metal/MetalFramebuffer.h
    )
endif()

# WebGPU-specific sources (must be before add_library)
if(METAGFX_USE_WEBGPU)
    list(APPEND RHI_SOURCES
        webgpu/WebGPUTypes.cpp
        webgpu/WebGPUDevice.cpp
        webgpu/WebGPUSwapChain.cpp
        webgpu/WebGPUBuffer.cpp
        webgpu/WebGPUTexture.cpp
        webgpu/WebGPUSampler.cpp
        webgpu/WebGPUShader.cpp
        webgpu/WebGPUPipeline.cpp
        webgpu/WebGPUCommandBuffer.cpp
        webgpu/WebGPUDescriptorSet.cpp
        webgpu/WebGPUFramebuffer.cpp
        webgpu/WebGPUSurfaceBridge.cpp
    )

    # Platform-specific surface bridges
    if(WIN32)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Windows.cpp)
    elseif(APPLE)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Metal.mm)
    elseif(EMSCRIPTEN)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Web.cpp)
    elseif(UNIX)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Linux.cpp)
    endif()

    list(APPEND RHI_HEADERS
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUTypes.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUDevice.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUSwapChain.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUTexture.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUSampler.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUShader.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUPipeline.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUCommandBuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUDescriptorSet.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUFramebuffer.h
        ${CMAKE_SOURCE_DIR}/include/metagfx/rhi/webgpu/WebGPUSurfaceBridge.h
    )
endif()

add_library(metagfx_rhi STATIC ${RHI_SOURCES} ${RHI_HEADERS})

target_include_directories(metagfx_rhi
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(metagfx_rhi
    PUBLIC
        metagfx_core
        SDL3::SDL3
)

# Vulkan
if(METAGFX_USE_VULKAN)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        target_link_libraries(metagfx_rhi PUBLIC Vulkan::Vulkan)
        target_compile_definitions(metagfx_rhi PUBLIC METAGFX_USE_VULKAN)
        message(STATUS "Vulkan found: ${Vulkan_VERSION}")
    else()
        message(WARNING "Vulkan SDK not found. Vulkan support will be disabled.")
        set(METAGFX_USE_VULKAN OFF CACHE BOOL "Vulkan support disabled (SDK not found)" FORCE)
    endif()
endif()

# Metal (macOS/iOS) using metal-cpp
if(METAGFX_USE_METAL)
    if(APPLE)
        # Add metal-cpp and SPIRV-Cross include paths
        target_include_directories(metagfx_rhi
            PRIVATE
                ${CMAKE_SOURCE_DIR}/external/metal-cpp
                ${CMAKE_SOURCE_DIR}/external/SPIRV-Cross
        )

        # Find and link Metal frameworks
        find_library(METAL_FRAMEWORK Metal REQUIRED)
        find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
        find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
        find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

        target_link_libraries(metagfx_rhi
            PUBLIC
                ${METAL_FRAMEWORK}
                ${METALKIT_FRAMEWORK}
                ${QUARTZCORE_FRAMEWORK}
                ${FOUNDATION_FRAMEWORK}
            PRIVATE
                spirv-cross-msl
                spirv-cross-glsl
        )

        target_compile_definitions(metagfx_rhi PUBLIC METAGFX_USE_METAL)
        message(STATUS "Metal support enabled (using metal-cpp)")
    else()
        message(WARNING "Metal is only available on Apple platforms. Metal support will be disabled.")
        set(METAGFX_USE_METAL OFF CACHE BOOL "Metal support disabled (not on Apple platform)" FORCE)
    endif()
endif()

# WebGPU (all platforms via Dawn) - Link configuration
if(METAGFX_USE_WEBGPU)
    # Add Dawn (includes Tint) and SPIRV-Cross include paths
    target_include_directories(metagfx_rhi
        PUBLIC
            /Users/Borja/dev/dawn/dawn_install/include  # Dawn and Tint headers
        PRIVATE
            ${CMAKE_SOURCE_DIR}/external/SPIRV-Cross
    )

    # Link Dawn, Tint, and SPIRV-Cross
    # Use absolute paths for Dawn/Tint libraries to avoid CMake target resolution issues
    # Order matters: libraries must come before their dependencies
    target_link_libraries(metagfx_rhi
        PRIVATE
            /Users/Borja/dev/dawn/dawn_install/lib/libwebgpu_dawn.a

            # Tint SPIR-V → WGSL transpilation libraries
            # High-level API
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_api.a

            # WGSL writer (IR → WGSL)
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_writer.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_writer_raise.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_writer_ir_to_program.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_writer_ast_printer.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_writer_common.a

            # SPIR-V reader (SPIR-V → IR) - NEW: Added main reader libraries
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_reader.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_reader_parser.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_reader_lower.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_reader_common.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_validate.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_ir.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_type.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_spirv_intrinsic.a

            # WGSL language support
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_program.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_resolver.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_sem.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_ast.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_intrinsic.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_wgsl_ir.a

            # Core IR
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_ir.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_ir_transform.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_ir_analysis.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_ir_binary.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_ir_type.a

            # Core language support
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_type.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_constant.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_lang_core_intrinsic.a

            # Utilities
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_text.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_text_generator.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_diagnostic.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_ice.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_rtti.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_symbol.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_containers.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_math.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_memory.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_bytes.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_command.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_file.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_macros.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_strconv.a
            /Users/Borja/dev/dawn/dawn_install/lib/libtint_utils_system.a

            spirv-cross-glsl     # Keep for analysis/fallback
    )

    target_compile_definitions(metagfx_rhi PUBLIC METAGFX_USE_WEBGPU)
    message(STATUS "WebGPU support enabled (using Google Dawn)")
endif()
