cmake_minimum_required(VERSION 3.28)

project(
    Elf3D
    VERSION 0.8.6
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

option(ELF3D_BUILD_ENGINE "Build the full Elf3D engine and rendering stack" ON)
option(ELF3D_BUILD_VIEWER "Build the Elf3D reference viewer" ON)
if(ELF3D_BUILD_VIEWER AND NOT ELF3D_BUILD_ENGINE)
    message(FATAL_ERROR "ELF3D_BUILD_VIEWER requires ELF3D_BUILD_ENGINE")
endif()

set(
    elf3d_local_validation
    "${CMAKE_SOURCE_DIR}/.internal/cmake/local-validation.cmake"
)
if(EXISTS "${elf3d_local_validation}")
    include("${elf3d_local_validation}")
endif()

include(cmake/compiler_options.cmake)
include(cmake/dependencies.cmake)
include(CTest)

add_subdirectory(modules)

if(ELF3D_BUILD_ENGINE)
    add_subdirectory(facade/elf3d)
endif()

if(ELF3D_BUILD_VIEWER)
    add_subdirectory(integrations/imgui)
    add_subdirectory(apps/viewer)
endif()

if(BUILD_TESTING AND ELF3D_BUILD_ENGINE)
    add_subdirectory(tests)
endif()
