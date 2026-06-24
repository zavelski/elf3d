cmake_minimum_required(VERSION 3.25)

project(
    Elf3D
    VERSION 0.2.0
    DESCRIPTION "Embeddable C++20 3D visualization engine"
    LANGUAGES C CXX
)

set(ELF3D_LICENSE_SPDX "MIT")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(ELF3D_BUILD_VIEWER "Build the Elf3D reference viewer" ON)

include(cmake/compiler_options.cmake)
include(cmake/dependencies.cmake)
include(CTest)

add_subdirectory(modules/core)
add_subdirectory(modules/math)
add_subdirectory(modules/interaction)
add_subdirectory(modules/assets)
add_subdirectory(modules/image)
add_subdirectory(modules/scene)
add_subdirectory(modules/clipping)
add_subdirectory(modules/navigation)
add_subdirectory(modules/picking)
add_subdirectory(modules/tools/selection)
add_subdirectory(modules/tools/visibility)
add_subdirectory(modules/tools/measurement)
add_subdirectory(modules/tools/clipping)
add_subdirectory(modules/gltf)
add_subdirectory(modules/graphics)
add_subdirectory(modules/backend_opengl)
add_subdirectory(modules/renderer)
add_subdirectory(modules/viewport)
add_subdirectory(facade/elf3d)

if(ELF3D_BUILD_VIEWER)
    add_subdirectory(integrations/imgui)
    add_subdirectory(apps/viewer)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
