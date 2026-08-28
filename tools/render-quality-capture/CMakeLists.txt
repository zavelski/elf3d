execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE ELF3D_CAPTURE_BUILD_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT ELF3D_CAPTURE_BUILD_REVISION)
    set(ELF3D_CAPTURE_BUILD_REVISION "unknown")
endif()

add_executable(
    elf3d_render_quality_capture
    render_quality_capture.cpp
)

target_compile_features(elf3d_render_quality_capture PRIVATE cxx_std_20)
target_compile_definitions(
    elf3d_render_quality_capture
    PRIVATE
        ELF3D_CAPTURE_BUILD_REVISION="${ELF3D_CAPTURE_BUILD_REVISION}"
)
target_link_libraries(
    elf3d_render_quality_capture
    PRIVATE
        elf3d::embed
        elf3d::third_party_glad
        elf3d_third_party_png
        glfw
        bcrypt
)
set_target_properties(elf3d_render_quality_capture PROPERTIES FOLDER "Elf3D/Tools")
elf3d_enable_warnings(elf3d_render_quality_capture)

if(BUILD_TESTING)
    add_test(
        NAME elf3d.render_quality_material_pixels
        COMMAND
            elf3d_render_quality_capture
            --model procedural-material
            --extent 1280x720
            --camera fit-front
            --output ${CMAKE_BINARY_DIR}/render-quality-test/material.png
            --metadata ${CMAKE_BINARY_DIR}/render-quality-test/material.json
            --validate-material true
    )
    set_tests_properties(
        elf3d.render_quality_material_pixels
        PROPERTIES
            SKIP_REGULAR_EXPRESSION "Hidden OpenGL 4\\.1 context initialization failed"
    )
endif()
