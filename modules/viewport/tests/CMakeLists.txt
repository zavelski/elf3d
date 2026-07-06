add_executable(
    elf3d_viewport_test
    offscreen_viewport_test.cpp
)

target_compile_features(elf3d_viewport_test PRIVATE cxx_std_20)
elf3d_link_object_libraries(
    elf3d_viewport_test
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_scene
        elf3d_clipping
        elf3d_interaction
        elf3d_navigation
        elf3d_picking
        elf3d_graphics
        elf3d_renderer
        elf3d_tool_selection
        elf3d_tool_visibility
        elf3d_tool_measurement
        elf3d_tool_clipping
        elf3d_viewport
)
set_target_properties(elf3d_viewport_test PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_viewport_test)

add_test(NAME elf3d.viewport_lifetime COMMAND elf3d_viewport_test)
