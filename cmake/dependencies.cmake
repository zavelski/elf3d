include(FetchContent)

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

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/archive/${ELF3D_GLM_COMMIT_SHA}.tar.gz
    URL_HASH SHA256=36eefbae41503e6822e8d9e1b4cc85c6a6621f17676f6258aaf0a64c2de0a09c
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)

FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG ${ELF3D_CGLTF_COMMIT_SHA}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(cgltf)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG ${ELF3D_STB_COMMIT_SHA}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(stb)

add_library(
    elf3d_third_party_glad
    STATIC
        ${PROJECT_SOURCE_DIR}/third_party/glad/include/glad/gl.h
        ${PROJECT_SOURCE_DIR}/third_party/glad/include/KHR/khrplatform.h
        ${PROJECT_SOURCE_DIR}/third_party/glad/src/gl.c
)
add_library(elf3d::third_party_glad ALIAS elf3d_third_party_glad)
target_compile_features(elf3d_third_party_glad PRIVATE c_std_11)
target_include_directories(
    elf3d_third_party_glad
    SYSTEM PUBLIC
        ${PROJECT_SOURCE_DIR}/third_party/glad/include
)
set_target_properties(
    elf3d_third_party_glad
    PROPERTIES
        FOLDER "Third Party"
        POSITION_INDEPENDENT_CODE ON
)
set_target_properties(
    glm-header-only
    PROPERTIES
        FOLDER "Third Party"
        SYSTEM ON
)

if(NOT ELF3D_BUILD_VIEWER)
    return()
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG ${ELF3D_IMGUI_COMMIT_SHA}
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${ELF3D_GLFW_COMMIT_SHA}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui glfw)

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
