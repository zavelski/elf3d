add_library(
    elf3d_picking
    OBJECT
        src/service.cpp
)
add_library(elf3d::picking ALIAS elf3d_picking)

target_compile_features(elf3d_picking PUBLIC cxx_std_20)
target_sources(
    elf3d_picking
    PUBLIC
        FILE_SET elf3d_picking_modules TYPE CXX_MODULES
        FILES
            src/elf.picking.cppm
)
target_include_directories(
    elf3d_picking
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_picking
    PUBLIC
        elf3d_scene
        elf3d_clipping
        elf3d_assets
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_picking
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_picking)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
