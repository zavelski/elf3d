add_library(
    elf3d_app
    STATIC
        include/elf3d/app/application.h
        include/elf3d/app/input.h
        include/elf3d/app/interaction.h
        src/application.cpp
        src/input_collector.cpp
        src/input_collector.h
        src/interaction.cpp
)
add_library(elf3d::app ALIAS elf3d_app)

target_compile_features(elf3d_app PUBLIC cxx_std_20)
target_include_directories(
    elf3d_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/facade/elf3d/src
        ${PROJECT_SOURCE_DIR}/integrations/imgui/src
)
target_link_libraries(
    elf3d_app
    PUBLIC
        elf3d::elf3d
    PRIVATE
        elf3d::imgui
        glfw
        OpenGL::GL
)
set_target_properties(
    elf3d_app
    PROPERTIES
        FOLDER "Elf3D/Frameworks"
        POSITION_INDEPENDENT_CODE ON
)

elf3d_enable_warnings(elf3d_app)

if(BUILD_TESTING)
    add_executable(
        elf3d_app_smoke_test
        tests/application_smoke_test.cpp
    )
    target_compile_features(elf3d_app_smoke_test PRIVATE cxx_std_20)
    target_link_libraries(elf3d_app_smoke_test PRIVATE elf3d::app)
    set_target_properties(elf3d_app_smoke_test PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(elf3d_app_smoke_test)
    add_test(NAME elf3d.application_smoke COMMAND elf3d_app_smoke_test)
    set_tests_properties(elf3d.application_smoke PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(
        elf3d_interaction_arbiter_tests
        tests/interaction_arbiter_test.cpp
    )
    target_compile_features(elf3d_interaction_arbiter_tests PRIVATE cxx_std_20)
    target_link_libraries(elf3d_interaction_arbiter_tests PRIVATE elf3d::app)
    set_target_properties(elf3d_interaction_arbiter_tests PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(elf3d_interaction_arbiter_tests)
    add_test(NAME elf3d.interaction_arbiter COMMAND elf3d_interaction_arbiter_tests)

    add_executable(
        elf3d_input_collector_tests
        tests/input_collector_test.cpp
    )
    target_compile_features(elf3d_input_collector_tests PRIVATE cxx_std_20)
    target_include_directories(elf3d_input_collector_tests PRIVATE src)
    target_link_libraries(elf3d_input_collector_tests PRIVATE elf3d::app)
    set_target_properties(elf3d_input_collector_tests PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(elf3d_input_collector_tests)
    add_test(NAME elf3d.input_collector COMMAND elf3d_input_collector_tests)
endif()
