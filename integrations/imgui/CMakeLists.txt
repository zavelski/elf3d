add_library(
    elf3d_imgui
    STATIC
        include/elf3d/imgui/context.h
        include/elf3d/imgui/texture.h
        src/context.cpp
        src/context_owner.h
        src/texture.cpp
        src/viewport_texture.h
)
add_library(elf3d::imgui ALIAS elf3d_imgui)

target_compile_features(elf3d_imgui PUBLIC cxx_std_20)
target_include_directories(
    elf3d_imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/facade/elf3d/src
)
target_link_libraries(
    elf3d_imgui
    PUBLIC
        elf3d::elf3d
        elf3d::third_party_imgui
)
set_target_properties(
    elf3d_imgui
    PROPERTIES
        FOLDER "Elf3D/Integrations"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_imgui)
