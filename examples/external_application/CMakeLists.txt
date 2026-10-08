cmake_minimum_required(VERSION 4.4.3)
project(Elf3DExternalApplication LANGUAGES C CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(ELF3D_SOURCE_DIR "" CACHE PATH "Path to the shared Elf3D source checkout")
if(NOT EXISTS "${ELF3D_SOURCE_DIR}/include/elf3d/elf3d.h")
    message(FATAL_ERROR "Set ELF3D_SOURCE_DIR to the Elf3D source checkout")
endif()

# This application owns its CRT, outputs, tests, and startup project.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/$<CONFIG>")
include(CTest)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT elf3d_external_application)

add_executable(elf3d_external_application main.cpp)
target_compile_features(elf3d_external_application PRIVATE cxx_std_20)
set_target_properties(
    elf3d_external_application
    PROPERTIES
        CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:elf3d_external_application>"
)
if(MSVC)
    target_compile_options(elf3d_external_application PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /Zc:preprocessor)
endif()

add_subdirectory("${ELF3D_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/elf3d" EXCLUDE_FROM_ALL)
target_link_libraries(elf3d_external_application PRIVATE elf3d::app)
add_custom_command(
    TARGET elf3d_external_application POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:elf3d>" "$<TARGET_FILE_DIR:elf3d_external_application>"
    VERBATIM
)

if(BUILD_TESTING)
    add_test(NAME external_application_smoke COMMAND elf3d_external_application)
    set_tests_properties(external_application_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
endif()
