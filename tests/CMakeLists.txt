add_executable(
    elf3d_public_api_test
    public_api_test.cpp
)

target_compile_features(elf3d_public_api_test PRIVATE cxx_std_20)
target_link_libraries(elf3d_public_api_test PRIVATE elf3d::elf3d)
set_target_properties(elf3d_public_api_test PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_public_api_test)

add_test(NAME elf3d.public_api_lifetime COMMAND elf3d_public_api_test)

if(TARGET glfw)
    add_executable(
        elf3d_opengl_render_smoke_test
        opengl_render_smoke_test.cpp
    )

    target_compile_features(elf3d_opengl_render_smoke_test PRIVATE cxx_std_20)
    target_link_libraries(
        elf3d_opengl_render_smoke_test
        PRIVATE
            elf3d::elf3d
            elf3d::third_party_glad
            glfw
    )
    set_target_properties(
        elf3d_opengl_render_smoke_test
        PROPERTIES
            FOLDER "Elf3D/Tests"
    )

    elf3d_enable_warnings(elf3d_opengl_render_smoke_test)

    add_test(NAME elf3d.opengl_render_smoke COMMAND elf3d_opengl_render_smoke_test)
    set_tests_properties(elf3d.opengl_render_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

add_executable(
    elf3d_gltf_probe
    gltf_corpus_probe.cpp
)

target_compile_features(elf3d_gltf_probe PRIVATE cxx_std_20)
target_link_libraries(elf3d_gltf_probe PRIVATE elf3d::elf3d)
set_target_properties(elf3d_gltf_probe PROPERTIES FOLDER "Elf3D/Tools")

elf3d_enable_warnings(elf3d_gltf_probe)

if(ELF3D_GLTF_CORPUS_DIR)
    add_test(
        NAME elf3d.gltf_local_corpus
        COMMAND elf3d_gltf_probe "${ELF3D_GLTF_CORPUS_DIR}"
    )
endif()

add_executable(
    elf3d_module_import_smoke
    module_import_smoke.cpp
)

target_compile_features(elf3d_module_import_smoke PRIVATE cxx_std_20)
elf3d_link_object_libraries(
    elf3d_module_import_smoke
    elf3d_core
    elf3d_math
    elf3d_graphics
    elf3d_backend_opengl
    elf3d_assets
    elf3d_image
    elf3d_scene
    elf3d_interaction
    elf3d_navigation
    elf3d_picking
    elf3d_tool_selection
    elf3d_tool_visibility
    elf3d_tool_measurement
    elf3d_clipping
    elf3d_tool_clipping
    elf3d_gltf
    elf3d_renderer
    elf3d_viewport
)
target_link_libraries(
    elf3d_module_import_smoke
    PRIVATE
        elf3d_third_party_cgltf
        elf3d_third_party_stb
        elf3d::third_party_glad
)
set_target_properties(elf3d_module_import_smoke PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_module_import_smoke)

add_test(NAME elf3d.module_import_smoke COMMAND elf3d_module_import_smoke)
