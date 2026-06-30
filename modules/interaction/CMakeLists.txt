add_library(
    elf3d_interaction
    OBJECT
        src/viewport_interaction.cpp
)
add_library(elf3d::interaction ALIAS elf3d_interaction)

target_compile_features(elf3d_interaction PUBLIC cxx_std_20)
target_sources(
    elf3d_interaction
    PUBLIC
        FILE_SET elf3d_interaction_modules TYPE CXX_MODULES
        FILES
            src/elf.interaction.cppm
)
target_include_directories(
    elf3d_interaction
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(elf3d_interaction PUBLIC elf3d_math elf3d_core)
set_target_properties(
    elf3d_interaction
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_interaction)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
