add_library(
    elf3d_clipping
    OBJECT
        include/elf3d/clipping/filter.h
        src/filter.cpp
)
add_library(elf3d::clipping ALIAS elf3d_clipping)

target_compile_features(elf3d_clipping PUBLIC cxx_std_20)
target_sources(
    elf3d_clipping
    PUBLIC
        FILE_SET elf3d_clipping_modules TYPE CXX_MODULES
        FILES
            src/elf.clipping.cppm
)
target_include_directories(
    elf3d_clipping
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_clipping
    PUBLIC
        elf3d_math
        elf3d_core
)
set_target_properties(
    elf3d_clipping
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_clipping)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
