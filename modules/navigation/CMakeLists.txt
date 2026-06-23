add_library(
    elf3d_navigation
    STATIC
        include/elf3d/navigation/orbit_navigation.h
        src/orbit_navigation.cpp
)
add_library(elf3d::navigation ALIAS elf3d_navigation)

target_compile_features(elf3d_navigation PUBLIC cxx_std_20)
target_include_directories(
    elf3d_navigation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_navigation
    PUBLIC
        elf3d_interaction
        elf3d_scene
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_navigation
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_navigation)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
