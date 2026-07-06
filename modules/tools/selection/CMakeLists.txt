add_library(
    elf3d_tool_selection
    OBJECT
        src/selection_controller.cpp
)
add_library(elf3d::tool_selection ALIAS elf3d_tool_selection)

target_compile_features(elf3d_tool_selection PUBLIC cxx_std_20)
target_sources(
    elf3d_tool_selection
    PUBLIC
        FILE_SET elf3d_tool_selection_modules TYPE CXX_MODULES
        FILES
            src/elf.tool.selection.cppm
)
target_include_directories(
    elf3d_tool_selection
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_tool_selection
    PUBLIC
        elf3d_core
        elf3d_clipping
        elf3d_picking
        elf3d_scene
)
set_target_properties(
    elf3d_tool_selection
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_tool_selection)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
