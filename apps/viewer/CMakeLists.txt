add_executable(
    elf3d_viewer
    WIN32
    src/main.cpp
    src/viewer_assets.cpp
    src/viewer_browser.cpp
    src/viewer_browser_context.cpp
    src/viewer_browser_platform.cpp
    src/viewer_chrome.cpp
    src/viewer_input_math.cpp
    src/viewer_panels.cpp
    src/viewer_performance_csv.cpp
    src/viewer_preferences.cpp
    src/viewer_retained_frame.cpp
    src/viewer_runtime.cpp
    src/viewer_scene_panels.cpp
    src/viewer_status.cpp
    src/viewer_viewport.cpp
)

target_compile_features(elf3d_viewer PRIVATE cxx_std_20)
target_compile_definitions(
    elf3d_viewer
    PRIVATE
        ELF3D_IMGUI_BRANCH="${ELF3D_IMGUI_BRANCH}"
        ELF3D_IMGUI_COMMIT_SHA="${ELF3D_IMGUI_COMMIT_SHA}"
        ELF3D_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(
    elf3d_viewer
    PRIVATE
        elf3d::elf3d
        elf3d::imgui
)
if(WIN32)
    target_link_libraries(
        elf3d_viewer
        PRIVATE
            advapi32
            ole32
            shell32
            windowscodecs
    )
endif()

add_custom_command(
    TARGET elf3d_viewer
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:elf3d_viewer>/assets"
)
set_target_properties(elf3d_viewer PROPERTIES FOLDER "Elf3D/Applications")

elf3d_enable_warnings(elf3d_viewer)

if(BUILD_TESTING)
    add_executable(
        elf3d_viewer_behavior_tests
        tests/viewer_behavior_test.cpp
        src/viewer_input_math.cpp
        src/viewer_retained_frame.cpp
    )
    target_compile_features(elf3d_viewer_behavior_tests PRIVATE cxx_std_20)
    target_include_directories(elf3d_viewer_behavior_tests PRIVATE src)
    target_link_libraries(
        elf3d_viewer_behavior_tests
        PRIVATE
            elf3d::elf3d
            elf3d::imgui
    )
    set_target_properties(elf3d_viewer_behavior_tests PROPERTIES FOLDER "Elf3D/Tests")
    elf3d_enable_warnings(elf3d_viewer_behavior_tests)
    add_test(NAME elf3d.viewer_behavior COMMAND elf3d_viewer_behavior_tests)
endif()
