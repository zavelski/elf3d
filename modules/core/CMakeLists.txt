add_library(
    elf3d_core
    OBJECT
        src/diagnostics.cpp
)
add_library(elf3d::core ALIAS elf3d_core)

target_compile_features(elf3d_core PUBLIC cxx_std_20)
target_sources(
    elf3d_core
    PUBLIC
        FILE_SET elf3d_core_modules TYPE CXX_MODULES
        FILES
            src/elf.core.cppm
)
target_compile_definitions(elf3d_core PUBLIC ELF3D_BUILDING_LIBRARY)
target_include_directories(
    elf3d_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
set_target_properties(
    elf3d_core
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
