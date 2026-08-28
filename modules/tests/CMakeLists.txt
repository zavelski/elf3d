function(elf3d_configure_group_test target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_compile_definitions(${target_name} PRIVATE ELF3D_STATIC)
    set_target_properties(${target_name} PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(${target_name})
endfunction()

add_executable(
    elf3d_foundation_tests
        foundation_test_driver.cpp
        ${PROJECT_SOURCE_DIR}/modules/core/tests/core_vocabulary_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/math/tests/math_conventions_test.cpp
)
elf3d_configure_group_test(elf3d_foundation_tests)
elf3d_link_object_libraries(
    elf3d_foundation_tests
        elf3d_foundation_modules
)
target_include_directories(
    elf3d_foundation_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/modules/math/include
)
target_link_libraries(elf3d_foundation_tests PRIVATE glm::glm)
add_test(NAME elf3d.core_vocabulary COMMAND elf3d_foundation_tests --case core_vocabulary)
add_test(NAME elf3d.math_conventions COMMAND elf3d_foundation_tests --case math_conventions)

if(ELF3D_BUILD_ENGINE)
    add_executable(
        elf3d_domain_tests
            domain_test_driver.cpp
            ${PROJECT_SOURCE_DIR}/modules/interaction/tests/viewport_interaction_test.cpp
            ${PROJECT_SOURCE_DIR}/modules/assets/tests/assets_test.cpp
            ${PROJECT_SOURCE_DIR}/modules/clipping/tests/clipping_test.cpp
            ${PROJECT_SOURCE_DIR}/modules/scene/tests/runtime_adapter_depth_test.cpp
            ${PROJECT_SOURCE_DIR}/modules/scene/tests/scene_test.cpp
    )
    elf3d_configure_group_test(elf3d_domain_tests)
    elf3d_link_object_libraries(
        elf3d_domain_tests
            elf3d_foundation_modules
            elf3d_model_modules
            elf3d_domain_modules
    )
    add_test(NAME elf3d.interaction COMMAND elf3d_domain_tests --case interaction)
    add_test(NAME elf3d.assets COMMAND elf3d_domain_tests --case assets)
    add_test(NAME elf3d.clipping COMMAND elf3d_domain_tests --case clipping)
    add_test(NAME elf3d.scene COMMAND elf3d_domain_tests --case scene)
    add_test(
        NAME elf3d.scene_runtime_adapter_depth
        COMMAND elf3d_domain_tests --case scene_runtime_adapter_depth
    )
endif()

add_executable(
    elf3d_import_tests
        import_test_driver.cpp
        ${PROJECT_SOURCE_DIR}/modules/image/tests/image_decoder_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/gltf/tests/gltf_exporter_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/gltf/tests/gltf_importer_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/gltf/tests/gltf_tangent_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/gltf/tests/gltf_metadata_round_trip_test.cpp
)
elf3d_configure_group_test(elf3d_import_tests)
target_compile_definitions(
    elf3d_import_tests
    PRIVATE
        ELF3D_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        ELF3D_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)
elf3d_link_object_libraries(
    elf3d_import_tests
        elf3d_foundation_modules
        elf3d_image_modules
        elf3d_model_modules
        elf3d_gltf_modules
)
target_link_libraries(
    elf3d_import_tests
    PRIVATE
        elf3d_third_party_png
        elf3d_third_party_jpeg
        elf3d_third_party_cgltf
)
add_test(NAME elf3d.image_decode COMMAND elf3d_import_tests --case image_decode)
add_test(NAME elf3d.model_gltf_export COMMAND elf3d_import_tests --case model_gltf_export)
add_test(NAME elf3d.model_gltf_import COMMAND elf3d_import_tests --case model_gltf_import)
add_test(NAME elf3d.model_gltf_tangent COMMAND elf3d_import_tests --case model_gltf_tangent)
add_test(NAME elf3d.model_gltf_metadata COMMAND elf3d_import_tests --case gltf_metadata)

add_executable(
    elf3d_model_tests
        model_test_driver.cpp
        ${PROJECT_SOURCE_DIR}/modules/model/tests/model_document_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/model/tests/model_processing_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/model/tests/model_tangent_test.cpp
)
elf3d_configure_group_test(elf3d_model_tests)
elf3d_link_object_libraries(
    elf3d_model_tests
        elf3d_foundation_modules
        elf3d_model_modules
)
target_include_directories(
    elf3d_model_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/modules/model/include
)
add_test(NAME elf3d.model_document COMMAND elf3d_model_tests --case model_document)
add_test(NAME elf3d.model_processing COMMAND elf3d_model_tests --case model_processing)
add_test(NAME elf3d.model_tangent COMMAND elf3d_model_tests --case model_tangent)

if(NOT ELF3D_BUILD_ENGINE)
    add_test(
        NAME elf3d.model_only_dependencies
        COMMAND
            ${CMAKE_COMMAND}
            "-DELF3D_MODEL_ONLY_BINARY_DIR=${CMAKE_BINARY_DIR}"
            "-DELF3D_MODEL_ONLY_OPENGL_TARGET_EXISTS=$<TARGET_EXISTS:OpenGL::GL>"
            -P
            "${PROJECT_SOURCE_DIR}/cmake/check-model-only-dependencies.cmake"
    )
endif()

if(ELF3D_BUILD_ENGINE)

add_executable(
    elf3d_interaction_tests
        interaction_test_driver.cpp
        ${PROJECT_SOURCE_DIR}/modules/navigation/tests/orbit_navigation_anchor_depth_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/navigation/tests/orbit_navigation_continuous_motion_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/navigation/tests/orbit_navigation_keyboard_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/navigation/tests/orbit_navigation_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/picking/tests/picking_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/selection/tests/selection_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/visibility/tests/visibility_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/clipping_runtime/tests/clipping_controller_test.cpp
)
elf3d_configure_group_test(elf3d_interaction_tests)
elf3d_link_object_libraries(
    elf3d_interaction_tests
        elf3d_foundation_modules
        elf3d_model_modules
        elf3d_domain_modules
        elf3d_interaction_modules
)
add_test(NAME elf3d.navigation COMMAND elf3d_interaction_tests --case navigation)
add_test(NAME elf3d.picking COMMAND elf3d_interaction_tests --case picking)
add_test(NAME elf3d.selection COMMAND elf3d_interaction_tests --case selection)
add_test(NAME elf3d.visibility COMMAND elf3d_interaction_tests --case visibility)
add_test(NAME elf3d.clipping_runtime COMMAND elf3d_interaction_tests --case clipping)

add_executable(
    elf3d_view_tests
        view_test_driver.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/renderer_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/renderer_environment_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/renderer_layout_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/renderer_frustum_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/renderer_document_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/renderer/tests/display_transform_test.cpp
        ${PROJECT_SOURCE_DIR}/modules/viewport/tests/offscreen_viewport_test.cpp
)
elf3d_configure_group_test(elf3d_view_tests)
elf3d_link_object_libraries(
    elf3d_view_tests
        elf3d_foundation_modules
        elf3d_model_modules
        elf3d_domain_modules
        elf3d_graphics_modules
        elf3d_interaction_modules
        elf3d_view_modules
)
add_test(NAME elf3d.renderer COMMAND elf3d_view_tests --case renderer)
add_test(NAME elf3d.renderer_layout COMMAND elf3d_view_tests --case renderer_layout)
add_test(NAME elf3d.renderer_frustum COMMAND elf3d_view_tests --case renderer_frustum)
add_test(NAME elf3d.renderer_document COMMAND elf3d_view_tests --case renderer_document)
add_test(NAME elf3d.display_transform COMMAND elf3d_view_tests --case display_transform)
add_test(NAME elf3d.viewport_lifetime COMMAND elf3d_view_tests --case viewport_lifetime)

endif()
