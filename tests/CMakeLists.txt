add_executable(
    elf3d_public_api_test
    public_api_test.cpp
)

add_library(
    elf3d_public_api_examples
    OBJECT
        ${PROJECT_SOURCE_DIR}/examples/embedded_viewer.cpp
        ${PROJECT_SOURCE_DIR}/examples/load_and_report.cpp
        ${PROJECT_SOURCE_DIR}/examples/procedural_scene.cpp
        ${PROJECT_SOURCE_DIR}/examples/picking_and_selection.cpp
        ${PROJECT_SOURCE_DIR}/examples/document_roundtrip.cpp
        ${PROJECT_SOURCE_DIR}/examples/multi_viewport.cpp
)
target_compile_features(elf3d_public_api_examples PRIVATE cxx_std_20)
target_link_libraries(elf3d_public_api_examples PRIVATE elf3d::elf3d)
set_target_properties(elf3d_public_api_examples PROPERTIES FOLDER "Elf3D/Examples")
elf3d_enable_warnings(elf3d_public_api_examples)

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

    add_executable(
        elf3d_model_quick_test
        model_quick_test.cpp
    )

    target_compile_features(elf3d_model_quick_test PRIVATE cxx_std_20)
    target_compile_definitions(
        elf3d_model_quick_test
        PRIVATE
            ELF3D_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    target_link_libraries(
        elf3d_model_quick_test
        PRIVATE
            elf3d::elf3d
            elf3d::third_party_glad
            glfw
    )
    set_target_properties(elf3d_model_quick_test PROPERTIES FOLDER "Elf3D/Tests")

    elf3d_enable_warnings(elf3d_model_quick_test)

    add_test(NAME elf3d.engine_model_integration COMMAND elf3d_model_quick_test)
    set_tests_properties(elf3d.engine_model_integration PROPERTIES SKIP_RETURN_CODE 77)
endif()

set(elf3d_internal_tests "${PROJECT_SOURCE_DIR}/.internal/cmake/internal-tests.cmake")
if(EXISTS "${elf3d_internal_tests}")
    include("${elf3d_internal_tests}")
endif()
