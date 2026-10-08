cmake_minimum_required(VERSION 4.4.3)

include(cmake/windows-toolchain.cmake)

set(ELF3D_BUILD_COMPONENT "all" CACHE STRING "Private checkout build composition")
set_property(CACHE ELF3D_BUILD_COMPONENT PROPERTY STRINGS all dependencies engine imgui viewer)
if(NOT ELF3D_BUILD_COMPONENT MATCHES "^(all|dependencies|engine|imgui|viewer)$")
    message(FATAL_ERROR "Unsupported ELF3D_BUILD_COMPONENT: ${ELF3D_BUILD_COMPONENT}")
endif()
set(elf3d_project_name Elf3D)
if(NOT ELF3D_BUILD_COMPONENT STREQUAL "all")
    if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        message(FATAL_ERROR "Manual component builds must be top-level builds")
    endif()
    string(SUBSTRING "${ELF3D_BUILD_COMPONENT}" 0 1 first_letter)
    string(TOUPPER "${first_letter}" first_letter)
    string(SUBSTRING "${ELF3D_BUILD_COMPONENT}" 1 -1 remaining_letters)
    set(elf3d_project_name "Elf3D-${first_letter}${remaining_letters}")
    if(ELF3D_BUILD_COMPONENT STREQUAL "imgui")
        set(elf3d_project_name Elf3D-ImGui)
    endif()
endif()

project(
    ${elf3d_project_name}
    VERSION 0.11.3
    DESCRIPTION "Embeddable C++20 3D visualization engine"
    LANGUAGES C CXX
)

elf3d_check_windows_compiler()

set(ELF3D_LICENSE_SPDX "MIT")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# First-party code uses textual headers; disable automatic module scanning.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

foreach(kind IN ITEMS RUNTIME LIBRARY ARCHIVE)
    if(NOT DEFINED CMAKE_${kind}_OUTPUT_DIRECTORY)
        if(kind STREQUAL "ARCHIVE")
            set(CMAKE_${kind}_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/$<CONFIG>")
        else()
            set(CMAKE_${kind}_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
        endif()
    endif()
endforeach()

include(cmake/compiler_options.cmake)
include(cmake/precompiled-headers.cmake)
include(cmake/ide.cmake)
if(NOT ELF3D_BUILD_COMPONENT STREQUAL "all")
    include(cmake/components.cmake)
    return()
endif()

if(PROJECT_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    include(CTest)
    set(elf3d_testing_default "${BUILD_TESTING}")
else()
    set(elf3d_testing_default OFF)
endif()
option(ELF3D_BUILD_TESTING "Build and register Elf3D tests" ${elf3d_testing_default})
if(ELF3D_BUILD_TESTING)
    enable_testing()
endif()

option(ELF3D_BUILD_ENGINE "Build the full Elf3D engine and rendering stack" ON)
option(
    ELF3D_BUILD_APP
    "Build the Elf3D standard desktop application framework"
    ${ELF3D_BUILD_ENGINE}
)
option(ELF3D_BUILD_VIEWER "Build the Elf3D reference viewer" ${PROJECT_IS_TOP_LEVEL})
option(ELF3D_BUILD_PERFORMANCE_BENCHMARK "Build the non-CTest render benchmark" OFF)
if((ELF3D_BUILD_APP OR ELF3D_BUILD_VIEWER) AND NOT ELF3D_BUILD_ENGINE)
    message(FATAL_ERROR "Elf3D application and viewer targets require ELF3D_BUILD_ENGINE")
endif()
if(ELF3D_BUILD_VIEWER AND NOT ELF3D_BUILD_APP)
    message(FATAL_ERROR "ELF3D_BUILD_VIEWER requires ELF3D_BUILD_APP")
endif()
if(ELF3D_BUILD_PERFORMANCE_BENCHMARK AND NOT ELF3D_BUILD_APP)
    message(FATAL_ERROR "ELF3D_BUILD_PERFORMANCE_BENCHMARK requires ELF3D_BUILD_APP")
endif()

set(
    elf3d_local_validation
    "${PROJECT_SOURCE_DIR}/.internal/cmake/local-validation.cmake"
)
if(PROJECT_IS_TOP_LEVEL AND EXISTS "${elf3d_local_validation}")
    include("${elf3d_local_validation}")
endif()

include(cmake/dependencies.cmake)

if(ELF3D_BUILD_ENGINE)
    add_subdirectory(tools/studio-environment-baker)
endif()

add_subdirectory(modules)

if(ELF3D_BUILD_ENGINE)
    add_subdirectory(facade/elf3d)
endif()

if(ELF3D_BUILD_APP)
    add_subdirectory(integrations/imgui)
    add_subdirectory(framework/app)
endif()

if(ELF3D_BUILD_VIEWER)
    add_subdirectory(apps/viewer)
    if(PROJECT_IS_TOP_LEVEL)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT elf3d_viewer)
    endif()
endif()

if(ELF3D_BUILD_TESTING AND ELF3D_BUILD_ENGINE)
    add_subdirectory(tests)
endif()

if(ELF3D_BUILD_PERFORMANCE_BENCHMARK)
    add_subdirectory(tools/render-benchmark)
    add_subdirectory(tools/render-quality-capture)
endif()
