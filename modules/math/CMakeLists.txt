add_library(
    elf3d_math
    OBJECT
        include/elf3d/math/conventions.h
        include/elf3d/math/detail/glm_helpers.h
        include/elf3d/math/glm_config.h
        src/conventions.cpp
)
add_library(elf3d::math ALIAS elf3d_math)

target_compile_features(elf3d_math PUBLIC cxx_std_20)
target_sources(
    elf3d_math
    PUBLIC
        FILE_SET elf3d_math_modules TYPE CXX_MODULES
        FILES
            src/elf.math.cppm
)
target_include_directories(
    elf3d_math
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(
    elf3d_math
    PUBLIC
        elf3d_core
        glm::glm
)
set_target_properties(
    elf3d_math
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_math)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
