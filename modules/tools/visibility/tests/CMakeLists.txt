add_executable(elf3d_tool_visibility_tests visibility_test.cpp)
elf3d_link_object_libraries(
    elf3d_tool_visibility_tests
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_scene
        elf3d_tool_visibility
)
set_target_properties(elf3d_tool_visibility_tests PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_tool_visibility_tests)
add_test(NAME elf3d_tool_visibility_tests COMMAND elf3d_tool_visibility_tests)
