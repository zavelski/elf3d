set(ELF3D_GLM_VERSION "1.0.3")
set(ELF3D_GLM_COMMIT_SHA "8d1fd52e5ab5590e2c81768ace50c72bae28f2ed")
set(ELF3D_GLAD_VERSION "v2.0.8")
set(ELF3D_GLAD_COMMIT_SHA "73db193f853e2ee079bf3ca8a64aa2eaf6459043")
set(ELF3D_IMGUI_BRANCH "docking")
set(ELF3D_IMGUI_COMMIT_SHA "036bf939b6f8d74ad76bcf926b757c56e68c54ff")
set(ELF3D_GLFW_VERSION "3.4")
set(ELF3D_GLFW_COMMIT_SHA "a74efa0d5628b74adc0426af4c5710e287fa7c2c")
set(ELF3D_CGLTF_VERSION "1.15")
set(ELF3D_CGLTF_COMMIT_SHA "360db1a95480fe102ae9c69b27c5d101167ff5ba")
set(ELF3D_STB_COMMIT_SHA "31c1ad37456438565541f4919958214b6e762fb4")

set(ELF3D_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")
set(cgltf_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/cgltf")
set(stb_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/stb")
set(imgui_SOURCE_DIR "${ELF3D_THIRD_PARTY_DIR}/imgui")

add_library(elf3d_third_party_glm INTERFACE)
add_library(glm::glm ALIAS elf3d_third_party_glm)
target_include_directories(
    elf3d_third_party_glm
    SYSTEM INTERFACE
        ${ELF3D_THIRD_PARTY_DIR}/glm
)
set_target_properties(
    elf3d_third_party_glm
    PROPERTIES
        FOLDER "Third Party"
)

add_library(
    elf3d_third_party_glad
    STATIC
        ${ELF3D_THIRD_PARTY_DIR}/glad/include/glad/gl.h
        ${ELF3D_THIRD_PARTY_DIR}/glad/include/KHR/khrplatform.h
        ${ELF3D_THIRD_PARTY_DIR}/glad/src/gl.c
)
add_library(elf3d::third_party_glad ALIAS elf3d_third_party_glad)
target_compile_features(elf3d_third_party_glad PRIVATE c_std_11)
target_include_directories(
    elf3d_third_party_glad
    SYSTEM PUBLIC
        ${ELF3D_THIRD_PARTY_DIR}/glad/include
)
set_target_properties(
    elf3d_third_party_glad
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

if(NOT ELF3D_BUILD_VIEWER)
    return()
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${ELF3D_THIRD_PARTY_DIR}/glfw
    ${CMAKE_BINARY_DIR}/third_party/glfw
    EXCLUDE_FROM_ALL
)

find_package(OpenGL REQUIRED)

add_library(
    elf3d_third_party_imgui
    STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(elf3d::third_party_imgui ALIAS elf3d_third_party_imgui)

target_compile_features(elf3d_third_party_imgui PUBLIC cxx_std_20)
target_include_directories(
    elf3d_third_party_imgui
    SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(
    elf3d_third_party_imgui
    PUBLIC
        glfw
        OpenGL::GL
)
set_target_properties(
    elf3d_third_party_imgui
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)

if(TARGET glfw)
    set_target_properties(glfw PROPERTIES FOLDER "Third Party")
endif()
