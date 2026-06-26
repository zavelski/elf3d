add_executable(elf3d_renderer_test renderer_test.cpp)
target_compile_features(elf3d_renderer_test PRIVATE cxx_std_20)
elf3d_link_object_libraries(
    elf3d_renderer_test
        elf3d_core
        elf3d_math
        elf3d_assets
        elf3d_scene
        elf3d_clipping
        elf3d_graphics
        elf3d_renderer
)
set_target_properties(elf3d_renderer_test PROPERTIES FOLDER "Elf3D/Tests")
elf3d_enable_warnings(elf3d_renderer_test)
add_test(NAME elf3d.renderer COMMAND elf3d_renderer_test)
