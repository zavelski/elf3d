add_executable(elf3d_interaction_test viewport_interaction_test.cpp)
target_compile_features(elf3d_interaction_test PRIVATE cxx_std_20)
elf3d_link_object_libraries(
    elf3d_interaction_test
        elf3d_core
        elf3d_math
        elf3d_interaction
)
set_target_properties(elf3d_interaction_test PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_interaction_test)
add_test(NAME elf3d.interaction COMMAND elf3d_interaction_test)
