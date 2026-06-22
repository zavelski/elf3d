add_executable(
    elf3d_viewport_test
    offscreen_viewport_test.cpp
)

target_compile_features(elf3d_viewport_test PRIVATE cxx_std_20)
target_link_libraries(elf3d_viewport_test PRIVATE elf3d_viewport)
set_target_properties(elf3d_viewport_test PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_viewport_test)

add_test(NAME elf3d.viewport_lifetime COMMAND elf3d_viewport_test)
