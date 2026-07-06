add_library(
    elf3d_scene
    OBJECT
        src/scene.cpp
        src/storage.cpp
)
add_library(elf3d::scene ALIAS elf3d_scene)

target_compile_features(elf3d_scene PUBLIC cxx_std_20)
target_sources(
    elf3d_scene
    PUBLIC
        FILE_SET elf3d_scene_modules TYPE CXX_MODULES
        FILES
            src/elf.scene.cppm
)
target_compile_definitions(elf3d_scene PRIVATE ELF3D_BUILDING_LIBRARY)
target_include_directories(
    elf3d_scene
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(elf3d_scene PUBLIC elf3d_core elf3d_assets elf3d_math)
set_target_properties(
    elf3d_scene
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_scene)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
