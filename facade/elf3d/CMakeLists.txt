add_library(
    elf3d
    SHARED
        src/engine.cpp
        src/scene.cpp
        src/viewport.cpp
)
add_library(elf3d::elf3d ALIAS elf3d)

target_compile_features(elf3d PUBLIC cxx_std_20)
target_compile_definitions(elf3d PRIVATE ELF3D_BUILDING_LIBRARY)
target_include_directories(
    elf3d
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
elf3d_link_object_libraries(
    elf3d
        elf3d_foundation_modules
        elf3d_domain_modules
        elf3d_image_modules
        elf3d_model_modules
        elf3d_gltf_modules
        elf3d_graphics_modules
        elf3d_opengl_modules
        elf3d_tools_modules
        elf3d_view_modules
)
set_target_properties(
    elf3d
    PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        FOLDER "Elf3D"
        OUTPUT_NAME elf3d
        VISIBILITY_INLINES_HIDDEN YES
)

elf3d_enable_warnings(elf3d)
