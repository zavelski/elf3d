add_executable(
    elf3d_viewer
    WIN32
    src/main.cpp
    src/viewer_assets.cpp
    src/viewer_browser.cpp
    src/viewer_browser_context.cpp
    src/viewer_browser_platform.cpp
    src/viewer_chrome.cpp
    src/viewer_panels.cpp
    src/viewer_preferences.cpp
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
