add_executable(elf3d_cmd
    src/main.cpp
    src/arguments.h src/arguments.cpp
    src/rotate_document.h src/rotate_document.cpp
    src/diagnostics.h src/diagnostics.cpp
)
target_link_libraries(elf3d_cmd PRIVATE elf3d::model)
target_compile_features(elf3d_cmd PRIVATE cxx_std_20)
set_target_properties(elf3d_cmd PROPERTIES
    FOLDER "Elf3D/Applications"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:elf3d_cmd>"
)
elf3d_enable_warnings(elf3d_cmd)
elf3d_group_sources(elf3d_cmd)
if(ELF3D_BUILD_COMPONENT STREQUAL "cmd")
    elf3d_check_prerequisites(elf3d_cmd elf3d_model ${model_dependencies})
endif()

if(ELF3D_BUILD_TESTING)
    add_executable(elf3d_cmd_tests
        tests/test_driver.cpp tests/arguments_test.cpp tests/rotation_test.cpp
        tests/fixture.h tests/fixture.cpp tests/round_trip_test.cpp
        src/arguments.cpp src/rotate_document.cpp src/diagnostics.cpp
    )
    target_include_directories(elf3d_cmd_tests PRIVATE src)
    target_link_libraries(elf3d_cmd_tests PRIVATE elf3d::model)
    set_target_properties(elf3d_cmd_tests PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(elf3d_cmd_tests)
    elf3d_group_sources(elf3d_cmd_tests)
    add_test(NAME elf3d.cmd_arguments COMMAND elf3d_cmd_tests arguments)
    add_test(NAME elf3d.cmd_rotation COMMAND elf3d_cmd_tests rotation)
    find_program(elf3d_cmd_pwsh NAMES pwsh REQUIRED)
    add_test(NAME elf3d.cmd_round_trip COMMAND "${elf3d_cmd_pwsh}" -NoProfile -File
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/round_trip.ps1"
        -Executable "$<TARGET_FILE:elf3d_cmd>" -Verifier "$<TARGET_FILE:elf3d_cmd_tests>"
        -WorkRoot "${CMAKE_CURRENT_BINARY_DIR}/tests/$<CONFIG>"
    )
    set_tests_properties(elf3d.cmd_round_trip PROPERTIES TIMEOUT 120)
endif()
