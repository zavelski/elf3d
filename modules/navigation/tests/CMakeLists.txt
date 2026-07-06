add_executable(elf3d_navigation_test orbit_navigation_test.cpp)
target_compile_features(elf3d_navigation_test PRIVATE cxx_std_20)
elf3d_link_object_libraries(
    elf3d_navigation_test
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_scene
        elf3d_interaction
        elf3d_navigation
)
set_target_properties(elf3d_navigation_test PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_navigation_test)
add_test(NAME elf3d.navigation COMMAND elf3d_navigation_test)
