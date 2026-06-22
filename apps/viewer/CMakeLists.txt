add_executable(
    elf3d_viewer
    src/main.cpp
)

target_compile_features(elf3d_viewer PRIVATE cxx_std_20)
target_compile_definitions(
    elf3d_viewer
    PRIVATE
        ELF3D_IMGUI_BRANCH="${ELF3D_IMGUI_BRANCH}"
        ELF3D_IMGUI_COMMIT_SHA="${ELF3D_IMGUI_COMMIT_SHA}"
)
target_link_libraries(
    elf3d_viewer
    PRIVATE
        elf3d::elf3d
        elf3d::imgui
)
set_target_properties(elf3d_viewer PROPERTIES FOLDER "Elf3D/Applications")

elf3d_enable_warnings(elf3d_viewer)
