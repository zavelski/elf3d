add_library(
    elf3d_viewport
    OBJECT
        src/offscreen_viewport.cpp
)
add_library(elf3d::viewport ALIAS elf3d_viewport)

target_compile_features(elf3d_viewport PUBLIC cxx_std_20)
target_sources(
    elf3d_viewport
    PUBLIC
        FILE_SET elf3d_viewport_modules TYPE CXX_MODULES
        FILES
            src/elf.viewport.cppm
)
target_link_libraries(
    elf3d_viewport
    PUBLIC
        elf3d_tool_selection
        elf3d_tool_visibility
        elf3d_tool_measurement
        elf3d_tool_clipping
        elf3d_picking
        elf3d_navigation
        elf3d_interaction
        elf3d_graphics
        elf3d_renderer
        elf3d_scene
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_viewport
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_viewport)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
