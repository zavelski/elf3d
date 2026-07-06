add_executable(elf3d_gltf_test gltf_importer_test.cpp)
target_compile_features(elf3d_gltf_test PRIVATE cxx_std_20)
target_compile_definitions(
    elf3d_gltf_test
    PRIVATE
        ELF3D_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
elf3d_link_object_libraries(
    elf3d_gltf_test
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_image
        elf3d_scene
        elf3d_gltf
)
target_link_libraries(
    elf3d_gltf_test
    PRIVATE
        elf3d_third_party_png
        elf3d_third_party_jpeg
        elf3d_third_party_cgltf
)
set_target_properties(elf3d_gltf_test PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_gltf_test)
add_test(NAME elf3d.gltf_import COMMAND elf3d_gltf_test)
