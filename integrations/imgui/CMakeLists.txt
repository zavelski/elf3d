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
elf3d_target_interface(elf3d_imgui)

target_include_directories(
    elf3d_imgui
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/facade/elf3d/src
)
set_target_properties(
    elf3d_imgui
    PROPERTIES
        FOLDER "Elf3D/Integrations"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_imgui)
elf3d_group_sources(elf3d_imgui)
if(ELF3D_BUILD_COMPONENT STREQUAL "imgui")
    elf3d_check_prerequisites(elf3d_imgui elf3d glfw)
endif()

elf3d_enable_pch(elf3d_imgui integration)
