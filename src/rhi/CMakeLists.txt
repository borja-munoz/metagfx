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

