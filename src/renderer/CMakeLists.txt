# ============================================================================
# src/renderer/CMakeLists.txt
# ============================================================================
set(RENDERER_SOURCES
    Renderer.cpp
    RasterizationRenderer.cpp
)

set(RENDERER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/renderer/Renderer.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/renderer/RasterizationRenderer.h
)

add_library(metagfx_renderer STATIC ${RENDERER_SOURCES} ${RENDERER_HEADERS})

target_include_directories(metagfx_renderer
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(metagfx_renderer
    PUBLIC
        metagfx_core
        metagfx_rhi
        metagfx_scene
)
