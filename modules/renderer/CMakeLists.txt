add_library(
    elf3d_renderer
    OBJECT
        src/renderer.cpp
)
add_library(elf3d::renderer ALIAS elf3d_renderer)

target_compile_features(elf3d_renderer PUBLIC cxx_std_20)
target_sources(
    elf3d_renderer
    PUBLIC
        FILE_SET elf3d_renderer_modules TYPE CXX_MODULES
        FILES
            src/elf.renderer.cppm
)
target_include_directories(
    elf3d_renderer
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_renderer
    PUBLIC
        elf3d_core
        elf3d_scene
        elf3d_clipping
        elf3d_assets
        elf3d_graphics
        elf3d_math
)
set_target_properties(
    elf3d_renderer
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_renderer)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
