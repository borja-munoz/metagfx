# ============================================================================
# src/scene/CMakeLists.txt
# ============================================================================
set(SCENE_SOURCES
    Camera.cpp
    Light.cpp
    Material.cpp
    Mesh.cpp
    Model.cpp
    Scene.cpp
    ShadowMap.cpp
)

set(SCENE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Camera.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Light.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Material.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Mesh.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Model.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/Scene.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/ShadowMap.h
)

add_library(metagfx_scene STATIC ${SCENE_SOURCES} ${SCENE_HEADERS})

target_include_directories(metagfx_scene
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(metagfx_scene
    PUBLIC
        metagfx_core
        metagfx_utils
        glm
        SDL3::SDL3
        assimp
)