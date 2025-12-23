# ============================================================================
# src/utils/CMakeLists.txt
# ============================================================================
set(UTILS_SOURCES
    TextureUtils.cpp
)

set(UTILS_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/utils/TextureUtils.h
)

add_library(metagfx_utils STATIC ${UTILS_SOURCES} ${UTILS_HEADERS})

target_include_directories(metagfx_utils
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(metagfx_utils
    PUBLIC
        metagfx_core
        metagfx_rhi
)
