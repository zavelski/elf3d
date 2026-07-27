# Verify the configured CPU-only product boundary from generated CMake metadata.

if(NOT DEFINED ELF3D_MODEL_ONLY_BINARY_DIR)
    message(FATAL_ERROR "ELF3D_MODEL_ONLY_BINARY_DIR is required")
endif()

set(cache_path "${ELF3D_MODEL_ONLY_BINARY_DIR}/CMakeCache.txt")
set(target_directories_path
    "${ELF3D_MODEL_ONLY_BINARY_DIR}/CMakeFiles/TargetDirectories.txt")

if(NOT EXISTS "${cache_path}")
    message(FATAL_ERROR "Model-only CMake cache is missing: ${cache_path}")
endif()
if(NOT EXISTS "${target_directories_path}")
    message(FATAL_ERROR
        "Model-only generated target list is missing: ${target_directories_path}")
endif()

file(READ "${cache_path}" cache_contents)
foreach(option ELF3D_BUILD_ENGINE ELF3D_BUILD_VIEWER)
    if(NOT cache_contents MATCHES "(^|\n)${option}:BOOL=OFF(\r?\n|$)")
        message(FATAL_ERROR "${option} must be OFF in a model-only configuration")
    endif()
endforeach()

if(NOT DEFINED ELF3D_MODEL_ONLY_OPENGL_TARGET_EXISTS)
    message(FATAL_ERROR "Model-only OpenGL target state is required")
endif()
if(ELF3D_MODEL_ONLY_OPENGL_TARGET_EXISTS)
    message(FATAL_ERROR "OpenGL::GL leaked into the model-only target graph")
endif()

file(READ "${target_directories_path}" target_directories)
string(REPLACE "\\" "/" target_directories "${target_directories}")

set(required_targets
    elf3d_foundation_modules
    elf3d_image_modules
    elf3d_model_modules
    elf3d_gltf_modules
    elf3d_model
    elf3d_foundation_tests
    elf3d_import_tests
    elf3d_model_tests
)
foreach(target IN LISTS required_targets)
    string(FIND "${target_directories}" "/${target}.dir" target_position)
    if(target_position EQUAL -1)
        message(FATAL_ERROR "Required model-only target is missing: ${target}")
    endif()
endforeach()

set(forbidden_targets
    elf3d_domain_modules
    elf3d_domain_tests
    elf3d_graphics_modules
    elf3d_opengl_modules
    elf3d_tools_modules
    elf3d_view_modules
    elf3d_third_party_glad
    elf3d_third_party_imgui
    elf3d
    elf3d_imgui
    elf3d_viewer
    glfw
    elf3d_public_api_test
    elf3d_opengl_render_smoke_test
    elf3d_model_quick_test
    elf3d_module_import_smoke
)
foreach(target IN LISTS forbidden_targets)
    string(FIND "${target_directories}" "/${target}.dir" target_position)
    if(NOT target_position EQUAL -1)
        message(FATAL_ERROR "Engine/viewer target leaked into model-only: ${target}")
    endif()
endforeach()

message(STATUS "Model-only target boundary verified")
