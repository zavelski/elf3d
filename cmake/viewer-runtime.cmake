# Attach file rules to the existing Viewer project; no deployment target.
set(runtime_directory "${PROJECT_BINARY_DIR}/bin/$<CONFIG>")
get_target_property(engine_debug_pdb elf3d ELF3D_PDB_DEBUG)
get_target_property(engine_release_pdb elf3d ELF3D_PDB_RELEASE)
set(engine_pdb "$<IF:$<CONFIG:Debug>,${engine_debug_pdb},${engine_release_pdb}>")
get_target_property(engine_debug_dll elf3d IMPORTED_LOCATION_DEBUG)
get_filename_component(engine_dll_name "${engine_debug_dll}" NAME)
set(deployed_dll "${runtime_directory}/${engine_dll_name}")
set(deployed_debug_pdb "$<$<CONFIG:Debug>:${runtime_directory}/elf3d.pdb>")
add_custom_command(
    OUTPUT "${deployed_dll}" "${deployed_debug_pdb}"
    BYPRODUCTS "$<$<CONFIG:Release>:${runtime_directory}/elf3d.pdb>"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${runtime_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:elf3d>" "${deployed_dll}"
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${engine_pdb}" "-DDESTINATION=${runtime_directory}/elf3d.pdb"
        "-DCONFIGURATION=$<CONFIG>" -P "${PROJECT_SOURCE_DIR}/cmake/copy-engine-symbols.cmake"
    DEPENDS "$<TARGET_FILE:elf3d>" "$<$<CONFIG:Debug>:${engine_debug_pdb}>"
    VERBATIM
)
target_sources(elf3d_viewer PRIVATE "${deployed_dll}" "${deployed_debug_pdb}")
file(GLOB_RECURSE viewer_assets CONFIGURE_DEPENDS LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
foreach(asset IN LISTS viewer_assets)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${asset}")
    set(destination "${runtime_directory}/assets/${relative}")
    get_filename_component(directory "${destination}" DIRECTORY)
    add_custom_command(OUTPUT "${destination}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${asset}" "${destination}"
        DEPENDS "${asset}" VERBATIM)
    target_sources(elf3d_viewer PRIVATE "${destination}")
endforeach()
