add_library(
    elf3d_tool_measurement
    OBJECT
        include/elf3d/tools/measurement/distance_measurement.h
        src/distance_measurement.cpp
)
add_library(elf3d::tool_measurement ALIAS elf3d_tool_measurement)

target_compile_features(elf3d_tool_measurement PUBLIC cxx_std_20)
target_sources(
    elf3d_tool_measurement
    PUBLIC
        FILE_SET elf3d_tool_measurement_modules TYPE CXX_MODULES
        FILES
            src/elf.tool.measurement.cppm
)
target_include_directories(
    elf3d_tool_measurement
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_tool_measurement
    PUBLIC
        elf3d_picking
        elf3d_clipping
        elf3d_interaction
        elf3d_scene
        elf3d_assets
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_tool_measurement
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_tool_measurement)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
