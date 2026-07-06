add_library(
    elf3d_backend_opengl
    OBJECT
        src/device.cpp
)
add_library(elf3d::backend_opengl ALIAS elf3d_backend_opengl)

target_compile_features(elf3d_backend_opengl PUBLIC cxx_std_20)
target_sources(
    elf3d_backend_opengl
    PUBLIC
        FILE_SET elf3d_backend_opengl_modules TYPE CXX_MODULES
        FILES
            src/elf.backend.opengl.cppm
)
target_link_libraries(
    elf3d_backend_opengl
    PUBLIC
        elf3d_core
        elf3d_graphics
    PRIVATE
        elf3d::third_party_glad
)
set_target_properties(
    elf3d_backend_opengl
    PROPERTIES
        FOLDER "Elf3D/Internal"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_backend_opengl)
