# ============================================================================
# src/core/CMakeLists.txt
# ============================================================================
set(CORE_SOURCES
    Logger.cpp
    Platform.cpp
)

set(CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/core/Logger.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/core/Platform.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/core/Types.h
)

add_library(metagfx_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(metagfx_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(metagfx_core
    PUBLIC
        glm
)
