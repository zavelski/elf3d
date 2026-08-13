add_library(
    elf3d_embed
    STATIC
        include/elf3d/embed/runtime.h
        src/runtime.cpp
)
add_library(elf3d::embed ALIAS elf3d_embed)

target_compile_features(elf3d_embed PUBLIC cxx_std_20)
target_include_directories(
    elf3d_embed
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/facade/elf3d/src
)
target_link_libraries(elf3d_embed PUBLIC elf3d::elf3d)
set_target_properties(
    elf3d_embed
    PROPERTIES
        FOLDER "Elf3D/Integrations"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_embed)
