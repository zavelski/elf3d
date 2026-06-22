add_executable(
    elf3d_public_api_test
    public_api_test.cpp
)

target_compile_features(elf3d_public_api_test PRIVATE cxx_std_20)
target_link_libraries(elf3d_public_api_test PRIVATE elf3d::elf3d)
set_target_properties(elf3d_public_api_test PROPERTIES FOLDER "Elf3D/Tests")

elf3d_enable_warnings(elf3d_public_api_test)

add_test(NAME elf3d.public_api_lifetime COMMAND elf3d_public_api_test)
