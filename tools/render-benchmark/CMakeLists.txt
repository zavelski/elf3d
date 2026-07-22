add_executable(
    elf3d_render_benchmark
    render_benchmark.cpp
)

target_compile_features(elf3d_render_benchmark PRIVATE cxx_std_20)
target_link_libraries(
    elf3d_render_benchmark
    PRIVATE
        elf3d::elf3d
        elf3d::third_party_glad
        glfw
)
set_target_properties(elf3d_render_benchmark PROPERTIES FOLDER "Elf3D/Tools")

elf3d_enable_warnings(elf3d_render_benchmark)
