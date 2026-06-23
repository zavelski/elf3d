add_library(
    elf3d_tool_visibility
    STATIC
        include/elf3d/tools/visibility/visibility_controller.h
        src/visibility_controller.cpp
)
add_library(elf3d::tool_visibility ALIAS elf3d_tool_visibility)

target_compile_features(elf3d_tool_visibility PUBLIC cxx_std_20)
target_include_directories(
    elf3d_tool_visibility
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_tool_visibility
    PUBLIC
        elf3d_scene
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_tool_visibility
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_tool_visibility)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
