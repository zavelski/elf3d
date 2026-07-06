add_executable(elf3d_tool_clipping_tests clipping_controller_test.cpp)
elf3d_link_object_libraries(
    elf3d_tool_clipping_tests
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_scene
        elf3d_clipping
        elf3d_tool_clipping
)
set_target_properties(elf3d_tool_clipping_tests PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_tool_clipping_tests)
add_test(NAME elf3d_tool_clipping_tests COMMAND elf3d_tool_clipping_tests)
