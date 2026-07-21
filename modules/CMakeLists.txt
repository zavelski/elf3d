function(elf3d_configure_module_group target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    target_compile_definitions(${target_name} PRIVATE ELF3D_BUILDING_LIBRARY)
    target_include_directories(
        ${target_name}
        PUBLIC
            ${PROJECT_SOURCE_DIR}/include
    )
    set_target_properties(
        ${target_name}
        PROPERTIES
            FOLDER "Elf3D/Internal"
            POSITION_INDEPENDENT_CODE ON
    )
    elf3d_enable_warnings(${target_name})
endfunction()

add_library(
    elf3d_foundation_modules
    OBJECT
        core/src/diagnostics.cpp
        math/include/elf3d/math/detail/glm_helpers.h
        math/include/elf3d/math/glm_config.h
        math/src/conventions.cpp
)
add_library(elf3d::foundation_modules ALIAS elf3d_foundation_modules)
elf3d_configure_module_group(elf3d_foundation_modules)
target_include_directories(
    elf3d_foundation_modules
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/math/include
)
target_sources(
    elf3d_foundation_modules
    PUBLIC
        FILE_SET elf3d_foundation_cxx_modules TYPE CXX_MODULES
        FILES
            core/src/elf.core.cppm
            math/src/elf.math.cppm
)
target_link_libraries(elf3d_foundation_modules PRIVATE glm::glm)

add_library(
    elf3d_model_modules
    OBJECT
        model/src/document.cpp
        model/src/document_metadata.cpp
        model/src/document_mutation.cpp
        model/src/document_validation.cpp
        model/src/document_views.cpp
        model/include/elf3d/model/detail/document_builder.h
        model/include/elf3d/model/detail/document_handle_access.h
        model/include/elf3d/model/detail/document_storage.h
        model/include/elf3d/model/detail/imported_metadata.h
)
add_library(elf3d::model_modules ALIAS elf3d_model_modules)
elf3d_configure_module_group(elf3d_model_modules)
target_include_directories(
    elf3d_model_modules
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/model/include
)
target_sources(
    elf3d_model_modules
    PUBLIC
        FILE_SET elf3d_model_cxx_modules TYPE CXX_MODULES
        FILES
            model/src/elf.model.cppm
)
target_link_libraries(elf3d_model_modules PUBLIC elf3d_foundation_modules)

if(ELF3D_BUILD_ENGINE)
    add_library(
        elf3d_domain_modules
        OBJECT
            interaction/src/viewport_interaction.cpp
            assets/src/storage.cpp
            clipping/src/filter.cpp
            scene/src/runtime_adapter.cpp
            scene/src/storage.cpp
            scene/src/visibility.cpp
    )
    add_library(elf3d::domain_modules ALIAS elf3d_domain_modules)
    elf3d_configure_module_group(elf3d_domain_modules)
    # MSVC derives module-dependency JSON names from the source basename. Keep
    # the duplicate storage.cpp outputs separated like their object files.
    set_source_files_properties(
        assets/src/storage.cpp
        PROPERTIES
            VS_SETTINGS
                "ModuleDependenciesFile=$(IntDir)assets/src/;SourceDependenciesFile=$(IntDir)assets/src/"
    )
    set_source_files_properties(
        scene/src/storage.cpp
        PROPERTIES
            VS_SETTINGS
                "ModuleDependenciesFile=$(IntDir)scene/src/;SourceDependenciesFile=$(IntDir)scene/src/"
    )
    target_sources(
        elf3d_domain_modules
        PUBLIC
            FILE_SET elf3d_domain_cxx_modules TYPE CXX_MODULES
            FILES
                interaction/src/elf.interaction.cppm
                assets/src/elf.assets.cppm
                clipping/src/elf.clipping.cppm
                scene/src/elf.scene.cppm
    )
    target_link_libraries(
        elf3d_domain_modules
        PUBLIC
            elf3d_foundation_modules
            elf3d_model_modules
    )
endif()

add_library(
    elf3d_image_modules
    OBJECT
        image/src/image_decoder.cpp
)
add_library(elf3d::image_modules ALIAS elf3d_image_modules)
elf3d_configure_module_group(elf3d_image_modules)
target_sources(
    elf3d_image_modules
    PUBLIC
        FILE_SET elf3d_image_cxx_modules TYPE CXX_MODULES
        FILES
            image/src/elf.image.cppm
)
target_link_libraries(
    elf3d_image_modules
    PUBLIC
        elf3d_foundation_modules
    PRIVATE
        elf3d_third_party_png
        elf3d_third_party_jpeg
)

add_library(
    elf3d_gltf_modules
    OBJECT
        gltf/src/exporter.cpp
        gltf/src/exporter_image.cpp
        gltf/src/exporter_json.cpp
        gltf/src/exporter_json_format.cpp
        gltf/src/exporter_json_writer.cpp
        gltf/src/exporter_output.cpp
        gltf/src/importer_encoding.cpp
        gltf/src/importer_assets.cpp
        gltf/src/importer_document.cpp
        gltf/src/importer_geometry.cpp
        gltf/src/importer_geometry_build.cpp
        gltf/src/importer_input.cpp
        gltf/src/importer_material.cpp
        gltf/src/importer_metadata.cpp
        gltf/src/importer_texture.cpp
        gltf/src/importer_validation.cpp
        gltf/src/importer.cpp
)
add_library(elf3d::gltf_modules ALIAS elf3d_gltf_modules)
elf3d_configure_module_group(elf3d_gltf_modules)
target_sources(
    elf3d_gltf_modules
    PUBLIC
        FILE_SET elf3d_gltf_cxx_modules TYPE CXX_MODULES
        FILES
            gltf/src/elf.gltf.cppm
)
target_include_directories(
    elf3d_gltf_modules
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/model/include
        ${cgltf_SOURCE_DIR}
)
target_link_libraries(
    elf3d_gltf_modules
    PUBLIC
        elf3d_foundation_modules
        elf3d_model_modules
    PRIVATE
        elf3d_image_modules
        elf3d_third_party_png
        elf3d_third_party_cgltf
)

add_library(elf3d_model STATIC)
add_library(elf3d::model ALIAS elf3d_model)
target_compile_features(elf3d_model PUBLIC cxx_std_20)
target_compile_definitions(elf3d_model PUBLIC ELF3D_STATIC)
target_include_directories(
    elf3d_model
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
elf3d_link_object_libraries(
    elf3d_model
        elf3d_foundation_modules
        elf3d_image_modules
        elf3d_model_modules
        elf3d_gltf_modules
)
target_link_libraries(
    elf3d_model
    PRIVATE
        elf3d_third_party_png
        elf3d_third_party_jpeg
        elf3d_third_party_cgltf
)
set_target_properties(
    elf3d_model
    PROPERTIES
        FOLDER "Elf3D"
        OUTPUT_NAME elf3d_model
        POSITION_INDEPENDENT_CODE ON
)
elf3d_enable_warnings(elf3d_model)

if(ELF3D_BUILD_ENGINE)

add_library(
    elf3d_graphics_modules
    OBJECT
        graphics/src/device.cpp
)
add_library(elf3d::graphics_modules ALIAS elf3d_graphics_modules)
elf3d_configure_module_group(elf3d_graphics_modules)
target_sources(
    elf3d_graphics_modules
    PUBLIC
        FILE_SET elf3d_graphics_cxx_modules TYPE CXX_MODULES
        FILES
            graphics/src/elf.graphics.cppm
)
target_link_libraries(
    elf3d_graphics_modules
    PUBLIC
        elf3d_foundation_modules
        elf3d_domain_modules
)

add_library(
    elf3d_opengl_modules
    OBJECT
        backend_opengl/src/device.cpp
        backend_opengl/src/device_indexed_draw.cpp
        backend_opengl/src/device_overlay.cpp
        backend_opengl/src/device_picking.cpp
        backend_opengl/src/device_resources.cpp
        backend_opengl/src/device_state.cpp
        backend_opengl/src/device_targets.cpp
)
add_library(elf3d::opengl_modules ALIAS elf3d_opengl_modules)
elf3d_configure_module_group(elf3d_opengl_modules)
target_sources(
    elf3d_opengl_modules
    PUBLIC
        FILE_SET elf3d_opengl_cxx_modules TYPE CXX_MODULES
        FILES
            backend_opengl/src/elf.backend.opengl.cppm
)
target_link_libraries(
    elf3d_opengl_modules
    PUBLIC
        elf3d_foundation_modules
        elf3d_graphics_modules
    PRIVATE
        elf3d::third_party_glad
)

add_library(
    elf3d_tools_modules
    OBJECT
        navigation/src/orbit_navigation_state.cpp
        navigation/src/orbit_navigation.cpp
        navigation/src/orbit_navigation_update.cpp
        picking/src/geometry_detail.h
        picking/src/service_geometry.cpp
        picking/src/service.cpp
        tools/selection/src/selection_controller.cpp
        tools/visibility/src/visibility_controller.cpp
        tools/measurement/src/distance_measurement.cpp
        tools/clipping/src/clipping_controller.cpp
)
add_library(elf3d::tools_modules ALIAS elf3d_tools_modules)
elf3d_configure_module_group(elf3d_tools_modules)
target_sources(
    elf3d_tools_modules
    PUBLIC
        FILE_SET elf3d_tools_cxx_modules TYPE CXX_MODULES
        FILES
            navigation/src/elf.navigation.cppm
            picking/src/elf.picking.cppm
            tools/selection/src/elf.tool.selection.cppm
            tools/visibility/src/elf.tool.visibility.cppm
            tools/measurement/src/elf.tool.measurement.cppm
            tools/clipping/src/elf.tool.clipping.cppm
)
target_link_libraries(
    elf3d_tools_modules
    PUBLIC
        elf3d_foundation_modules
        elf3d_domain_modules
        elf3d_graphics_modules
)

add_library(
    elf3d_view_modules
    OBJECT
        renderer/src/gpu_picking.cpp
        renderer/src/model_rendering.cpp
        renderer/src/render_list.cpp
        renderer/src/renderer.cpp
        viewport/src/offscreen_viewport.cpp
        viewport/src/offscreen_viewport_picking.cpp
)
add_library(elf3d::view_modules ALIAS elf3d_view_modules)
elf3d_configure_module_group(elf3d_view_modules)
target_sources(
    elf3d_view_modules
    PUBLIC
        FILE_SET elf3d_view_cxx_modules TYPE CXX_MODULES
        FILES
            renderer/src/elf.renderer.cppm
            viewport/src/elf.viewport.cppm
)
target_link_libraries(
    elf3d_view_modules
    PUBLIC
        elf3d_foundation_modules
        elf3d_domain_modules
        elf3d_graphics_modules
        elf3d_tools_modules
)

endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
