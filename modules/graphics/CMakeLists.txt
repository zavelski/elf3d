add_library(
    elf3d_graphics
    STATIC
        include/elf3d/graphics/device.h
        include/elf3d/graphics/texture_handle_access.h
        src/device.cpp
)
add_library(elf3d::graphics ALIAS elf3d_graphics)

target_compile_features(elf3d_graphics PUBLIC cxx_std_20)
target_include_directories(
    elf3d_graphics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(elf3d_graphics PUBLIC elf3d_clipping elf3d_core)
set_target_properties(
    elf3d_graphics
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_graphics)
