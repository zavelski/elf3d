add_library(
    elf3d
    SHARED
        src/engine.cpp
        src/engine_access.h
        src/scene.cpp
        src/viewport.cpp
        src/viewport_navigation.cpp
)
add_library(elf3d::elf3d ALIAS elf3d)

if(WIN32)
    enable_language(RC)
    file(TO_CMAKE_PATH
        "${PROJECT_SOURCE_DIR}/modules/renderer/assets/studio_environment_v3.ibl"
        ELF3D_STUDIO_ENVIRONMENT_RESOURCE_PATH
    )
    configure_file(
        src/elf3d_resources.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/elf3d_resources.rc"
        @ONLY
    )
    target_sources(
        elf3d
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/elf3d_resources.rc"
            src/resource_ids.h
            src/studio_environment_resource.h
            src/studio_environment_resource_windows.cpp
    )
else()
    set(
        ELF3D_STUDIO_ENVIRONMENT_PORTABLE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/studio_environment_resource_portable.cpp"
    )
    execute_process(
        COMMAND
            ${CMAKE_COMMAND}
            "-DELF3D_EMBED_INPUT=${PROJECT_SOURCE_DIR}/modules/renderer/assets/studio_environment_v3.ibl"
            "-DELF3D_EMBED_OUTPUT=${ELF3D_STUDIO_ENVIRONMENT_PORTABLE_SOURCE}"
            -P
            "${PROJECT_SOURCE_DIR}/cmake/embed-studio-environment.cmake"
        RESULT_VARIABLE ELF3D_STUDIO_ENVIRONMENT_EMBED_RESULT
    )
    if(NOT ELF3D_STUDIO_ENVIRONMENT_EMBED_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not generate the portable embedded studio environment")
    endif()
    target_sources(
        elf3d
        PRIVATE
            ${ELF3D_STUDIO_ENVIRONMENT_PORTABLE_SOURCE}
            src/studio_environment_resource.h
    )
endif()

target_compile_features(elf3d PUBLIC cxx_std_20)
target_compile_definitions(elf3d PRIVATE ELF3D_BUILDING_LIBRARY)
target_include_directories(
    elf3d
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
elf3d_link_object_libraries(
    elf3d
        elf3d_foundation_modules
        elf3d_domain_modules
        elf3d_image_modules
        elf3d_model_modules
        elf3d_gltf_modules
        elf3d_graphics_modules
        elf3d_opengl_modules
        elf3d_interaction_modules
        elf3d_view_modules
)
target_link_libraries(elf3d PRIVATE elf3d_third_party_mikktspace)
set_target_properties(
    elf3d
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        FOLDER "Elf3D"
        OUTPUT_NAME elf3d
        VISIBILITY_INLINES_HIDDEN YES
)

elf3d_enable_warnings(elf3d)
