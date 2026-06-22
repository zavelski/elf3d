add_executable(
    elf3d_math_test
    math_conventions_test.cpp
)

target_compile_features(elf3d_math_test PRIVATE cxx_std_20)
target_link_libraries(elf3d_math_test PRIVATE elf3d_math)
set_target_properties(elf3d_math_test PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_math_test)

add_test(NAME elf3d.math_conventions COMMAND elf3d_math_test)
