add_library(
    elf3d
    SHARED
        src/engine.cpp
)
add_library(elf3d::elf3d ALIAS elf3d)

target_compile_features(elf3d PUBLIC cxx_std_20)
target_compile_definitions(elf3d PRIVATE ELF3D_BUILDING_LIBRARY)
target_include_directories(
    elf3d
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(elf3d PRIVATE elf3d_core)
target_link_libraries(
    elf3d
    PRIVATE
        elf3d_math
        elf3d_graphics
        elf3d_backend_opengl
        elf3d_viewport
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
