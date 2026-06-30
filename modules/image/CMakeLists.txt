add_library(
    elf3d_third_party_stb
    STATIC
        src/stb_image_implementation.cpp
)
target_compile_features(elf3d_third_party_stb PRIVATE cxx_std_20)
target_include_directories(elf3d_third_party_stb SYSTEM PRIVATE ${stb_SOURCE_DIR})
set_target_properties(
    elf3d_third_party_stb
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

add_library(
    elf3d_image
    OBJECT
        src/image_decoder.cpp
)
add_library(elf3d::image ALIAS elf3d_image)

target_compile_features(elf3d_image PUBLIC cxx_std_20)
target_sources(
    elf3d_image
    PUBLIC
        FILE_SET elf3d_image_modules TYPE CXX_MODULES
        FILES
            src/elf.image.cppm
)
target_include_directories(
    elf3d_image
    PRIVATE
        ${stb_SOURCE_DIR}
)
target_link_libraries(
    elf3d_image
    PUBLIC
        elf3d_core
    PRIVATE
        elf3d_third_party_stb
)
set_target_properties(
    elf3d_image
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_image)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
