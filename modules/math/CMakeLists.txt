add_library(
    elf3d_math
    STATIC
        include/elf3d/math/conventions.h
        include/elf3d/math/glm_config.h
        src/conventions.cpp
)
add_library(elf3d::math ALIAS elf3d_math)

target_compile_features(elf3d_math PUBLIC cxx_std_20)
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
