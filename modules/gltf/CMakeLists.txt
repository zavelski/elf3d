add_library(
    elf3d_third_party_cgltf
    STATIC
        src/cgltf_implementation.cpp
)
target_compile_features(elf3d_third_party_cgltf PRIVATE cxx_std_20)
target_include_directories(elf3d_third_party_cgltf SYSTEM PRIVATE ${cgltf_SOURCE_DIR})
set_target_properties(
    elf3d_third_party_cgltf
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

add_library(
    elf3d_gltf
    OBJECT
        src/importer.cpp
)
add_library(elf3d::gltf ALIAS elf3d_gltf)

target_compile_features(elf3d_gltf PUBLIC cxx_std_20)
target_sources(
    elf3d_gltf
    PUBLIC
        FILE_SET elf3d_gltf_modules TYPE CXX_MODULES
        FILES
            src/elf.gltf.cppm
)
target_include_directories(
    elf3d_gltf
    PRIVATE
        ${cgltf_SOURCE_DIR}
)
target_link_libraries(
    elf3d_gltf
    PUBLIC
        elf3d_scene
        elf3d_assets
        elf3d_math
        elf3d_core
    PRIVATE
        elf3d_third_party_cgltf
        elf3d_image
)
set_target_properties(
    elf3d_gltf
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_gltf)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
