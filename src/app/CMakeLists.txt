# ============================================================================
# src/app/CMakeLists.txt
# ============================================================================
set(APP_SOURCES
    main.cpp
    Application.cpp
)

set(APP_HEADERS
    Application.h
)

add_executable(metagfx ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(metagfx
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add metal-cpp include path if Metal is enabled
if(METAGFX_USE_METAL)
    target_include_directories(metagfx PRIVATE ${CMAKE_SOURCE_DIR}/external/metal-cpp)
endif()

target_link_libraries(metagfx
    PRIVATE
        metagfx_renderer
        SDL3::SDL3
        imgui_backends  # ImGui with SDL3 and Vulkan backends
)

# Copy SDL3 DLL on Windows
if(WIN32)
    add_custom_command(TARGET metagfx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:metagfx>
    )
endif()

set_target_properties(metagfx PROPERTIES OUTPUT_NAME "MetaGFX")
