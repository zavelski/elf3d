add_executable(
    elf3d_studio_environment_baker
    studio_environment_baker.cpp
)

target_compile_features(elf3d_studio_environment_baker PRIVATE cxx_std_20)
set_target_properties(
    elf3d_studio_environment_baker
    PROPERTIES
        FOLDER "Elf3D/Tools"
)
elf3d_enable_warnings(elf3d_studio_environment_baker)

if(BUILD_TESTING)
    add_test(
        NAME elf3d.studio_environment_bake
        COMMAND
            elf3d_studio_environment_baker
            --verify
            "${PROJECT_SOURCE_DIR}/modules/renderer/assets/studio_environment_v3.ibl"
    )
    set_tests_properties(elf3d.studio_environment_bake PROPERTIES TIMEOUT 300)
endif()
