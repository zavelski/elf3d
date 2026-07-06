add_library(
    elf3d_assets
    OBJECT
        src/storage.cpp
)
add_library(elf3d::assets ALIAS elf3d_assets)

target_compile_features(elf3d_assets PUBLIC cxx_std_20)
target_sources(
    elf3d_assets
    PUBLIC
        FILE_SET elf3d_assets_modules TYPE CXX_MODULES
        FILES
            src/elf.assets.cppm
)
target_include_directories(
    elf3d_assets
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(elf3d_assets PUBLIC elf3d_core elf3d_math)
set_target_properties(
    elf3d_assets
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_assets)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
