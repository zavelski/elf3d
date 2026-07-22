module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/model/detail/imported_metadata.h>

#include "importer_internal.hpp"

#include <cgltf.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.math;
import elf.model;

namespace elf3d::gltf::importer_detail {

[[nodiscard]] Result<void> attach_document_metadata(const cgltf_data& data,
                                                    const ImportedDocumentIds& ids,
                                                    Document& document,
                                                    std::vector<ModelLoadDiagnostic>& diagnostics) {
    const importer_metadata::ImportedMetadataIds metadata_ids{
        std::span{ids.scenes},     std::span{ids.nodes},     std::span{ids.meshes},
        std::span{ids.primitives}, std::span{ids.materials}, std::span{ids.images.ids},
        std::span{ids.textures},   std::span{ids.samplers}};
    return importer_metadata::attach_imported_metadata(data, metadata_ids, document, diagnostics);
}

struct DocumentLoadState {
    bool is_glb = false;
    std::size_t repaired_signed_buffer_fields = 0U;
    std::vector<std::byte> source;
    AllocationContext allocation_context;
    BufferLoadContext buffer_context;
    cgltf_options parser_options{};
    CgltfData data{nullptr};
};

[[nodiscard]] Result<bool> source_is_glb(const std::filesystem::path& path) {
    const std::string extension = lower_extension(path);
    if (extension != ".gltf" && extension != ".glb") {
        return Error{ErrorCode::unsupported_scene_format,
                     "Scene loading currently supports only .gltf and .glb files"};
    }
    return extension == ".glb";
}

void configure_parser_options(DocumentLoadState& state) noexcept {
    state.parser_options.memory.alloc_func = bounded_allocate;
    state.parser_options.memory.free_func = bounded_deallocate;
    state.parser_options.memory.user_data = &state.allocation_context;
    state.parser_options.file.read = read_external_file;
    state.parser_options.file.release = release_external_file;
    state.parser_options.file.user_data = &state.buffer_context;
}

[[nodiscard]] Result<void> parse_source(const std::filesystem::path& path,
                                        DocumentLoadState& state) {
    Result<std::vector<std::byte>> source = read_source(path);
    if (!source) {
        return source.error();
    }
    state.source = std::move(source).value();
    configure_parser_options(state);
    cgltf_data* parsed = nullptr;
    const cgltf_result result =
        cgltf_parse(&state.parser_options, state.source.data(), state.source.size(), &parsed);
    if (result != cgltf_result_success) {
        return parse_error(result, state.is_glb);
    }
    state.data.reset(parsed);
    if ((state.is_glb && state.data->file_type != cgltf_file_type_glb) ||
        (!state.is_glb && state.data->file_type != cgltf_file_type_gltf)) {
        return Error{state.is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                     "The source contents do not match the file extension"};
    }
    state.repaired_signed_buffer_fields = repair_signed_glb_buffer_layout(*state.data);
    decode_image_json_strings(*state.data);
    return {};
}

[[nodiscard]] Error initial_validation_error(cgltf_result result) {
    return Error{result == cgltf_result_data_too_short ? ErrorCode::invalid_buffer_range
                                                       : ErrorCode::gltf_validation_failed,
                 result == cgltf_result_data_too_short
                     ? "A glTF buffer view or accessor exceeds its declared buffer range"
                     : "cgltf structural validation rejected the source"};
}

[[nodiscard]] Result<void> validate_parsed_document(cgltf_data& data) {
    if (const Result<void> textures = validate_texture_inputs(data); !textures) {
        return textures.error();
    }
    if (const cgltf_result result = cgltf_validate(&data); result != cgltf_result_success) {
        return initial_validation_error(result);
    }
    if (const Result<void> extensions = validate_required_extensions(data); !extensions) {
        return extensions.error();
    }
    if (const Result<void> limits = validate_resource_limits(data); !limits) {
        return limits.error();
    }
    if (const Result<void> hierarchy = validate_node_hierarchy(data); !hierarchy) {
        return hierarchy.error();
    }
    return validate_buffer_uris(data);
}

[[nodiscard]] Result<void> load_external_buffers(const std::filesystem::path& path,
                                                 DocumentLoadState& state) {
    const std::string source_path = path_to_utf8(path);
    const cgltf_result result =
        cgltf_load_buffers(&state.parser_options, state.data.get(), source_path.c_str());
    if (result == cgltf_result_success) {
        return {};
    }
    if (state.buffer_context.error_code.has_value()) {
        return Error{*state.buffer_context.error_code, state.buffer_context.diagnostic};
    }
    return Error{result == cgltf_result_file_not_found ? ErrorCode::missing_external_buffer
                                                       : ErrorCode::invalid_buffer_range,
                 "cgltf could not load or decode a declared buffer"};
}

struct ReachableFeatureUse {
    bool skin = false;
    bool morph_targets = false;
    bool instancing = false;
};

[[nodiscard]] ReachableFeatureUse
inspect_reachable_features(const cgltf_data& data, const std::vector<bool>& reachable) noexcept {
    ReachableFeatureUse result;
    for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
        if (!reachable[node_index]) {
            continue;
        }
        const cgltf_node& node = data.nodes[node_index];
        result.skin = result.skin || node.skin != nullptr;
        result.instancing = result.instancing || node.has_mesh_gpu_instancing != 0;
        if (node.mesh != nullptr) {
            for (cgltf_size index = 0; index < node.mesh->primitives_count; ++index) {
                result.morph_targets =
                    result.morph_targets || node.mesh->primitives[index].targets_count != 0;
            }
        }
    }
    return result;
}

void add_extension_diagnostics(const cgltf_data& data, ModelLoadReport& report) {
    for (cgltf_size index = 0; index < data.extensions_used_count; ++index) {
        const char* extension = data.extensions_used[index];
        if (extension != nullptr && !extension_has_full_support(extension)) {
            add_optional_extension_diagnostic(report.diagnostics, extension);
        }
    }
    if (data.animations_count != 0) {
        add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::animation,
                       ModelLoadDiagnosticCode::ignored_animation,
                       "Animation clips and channels are not imported; static node transforms "
                       "were used");
    }
}

void add_feature_diagnostics(ReachableFeatureUse features, ModelLoadReport& report) {
    if (features.skin) {
        add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::animation,
                       ModelLoadDiagnosticCode::ignored_skin,
                       "Skinning is not imported; meshes use their undeformed bind geometry");
    }
    if (features.morph_targets) {
        add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::animation,
                       ModelLoadDiagnosticCode::ignored_morph_targets,
                       "Morph targets and weights are not imported; base mesh geometry is used");
    }
    if (features.instancing) {
        add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::geometry,
                       ModelLoadDiagnosticCode::ignored_instancing,
                       "EXT_mesh_gpu_instancing attributes are not expanded; the base node is "
                       "loaded once");
    }
}

[[nodiscard]] ModelLoadReport build_load_report(const cgltf_data& data,
                                                const std::vector<bool>& reachable,
                                                std::size_t repaired_signed_buffer_fields) {
    ModelLoadReport report;
    if (repaired_signed_buffer_fields != 0U) {
        add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::scene,
                       ModelLoadDiagnosticCode::repaired_signed_buffer_layout,
                       "Recovered signed 32-bit GLB buffer sizes and offsets from the complete "
                       "sequential BIN layout",
                       std::to_string(repaired_signed_buffer_fields) + " repaired fields");
    }
    add_extension_diagnostics(data, report);
    add_feature_diagnostics(inspect_reachable_features(data, reachable), report);
    return report;
}

struct DocumentConstructionState {
    const cgltf_data& data;
    const std::vector<bool>& reachable;
    const std::filesystem::path& gltf_path;
    const ModelLoadOptions& options;
    std::vector<ModelLoadDiagnostic>& diagnostics;
    DocumentBuilder builder;
    ImportedDocumentIds imported_ids;
};

[[nodiscard]] Result<DocumentSceneId>
create_implicit_document_scene(DocumentConstructionState& state) {
    Result<DocumentSceneId> scene = state.builder.create_scene();
    if (!scene) {
        return scene.error();
    }
    state.imported_ids.scenes.push_back(scene.value());
    const Result<void> cleared = state.builder.clear_default_scene();
    if (!cleared) {
        return cleared.error();
    }
    return scene.value();
}

[[nodiscard]] Result<void> create_authored_document_scenes(DocumentConstructionState& state) {
    for (cgltf_size index = 0; index < state.data.scenes_count; ++index) {
        const cgltf_scene& source = state.data.scenes[index];
        Result<DocumentSceneId> scene = state.builder.create_scene(
            source.name != nullptr ? std::string_view{source.name} : std::string_view{});
        if (!scene) {
            return scene.error();
        }
        state.imported_ids.scenes.push_back(scene.value());
    }
    return {};
}

[[nodiscard]] Result<DocumentSceneId>
select_default_document_scene(DocumentConstructionState& state) {
    if (state.data.scene == nullptr) {
        const Result<void> cleared = state.builder.clear_default_scene();
        if (!cleared) {
            return cleared.error();
        }
        return state.imported_ids.scenes[0];
    }
    if (state.data.scene < state.data.scenes ||
        state.data.scene >= state.data.scenes + state.data.scenes_count) {
        return Error{ErrorCode::invalid_node_hierarchy,
                     "The glTF default scene is outside the scene table"};
    }
    const std::size_t index = static_cast<std::size_t>(state.data.scene - state.data.scenes);
    const Result<void> selected = state.builder.set_default_scene(state.imported_ids.scenes[index]);
    if (!selected) {
        return selected.error();
    }
    return state.imported_ids.scenes[index];
}

[[nodiscard]] Result<DocumentSceneId> create_document_scenes(DocumentConstructionState& state) {
    state.imported_ids.scenes.reserve(state.data.scenes_count != 0 ? state.data.scenes_count : 1U);
    if (state.data.scenes_count == 0) {
        return create_implicit_document_scene(state);
    }
    const Result<void> created = create_authored_document_scenes(state);
    if (!created) {
        return created.error();
    }
    return select_default_document_scene(state);
}

[[nodiscard]] Result<void> capture_node_transforms(DocumentConstructionState& state,
                                                   std::vector<bool>& skipped_nodes,
                                                   std::vector<Float4x4>& local_matrices) {
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        if (!state.reachable[index]) {
            continue;
        }
        const cgltf_node& node = state.data.nodes[index];
        float local_values[16]{};
        cgltf_node_transform_local(&node, local_values);
        std::copy_n(local_values, local_matrices[index].elements.size(),
                    local_matrices[index].elements.begin());
        if (math::is_valid_affine_matrix(local_matrices[index])) {
            continue;
        }
        skipped_nodes[index] = true;
        const std::string context = node_context(node, index);
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::scene,
                       ModelLoadDiagnosticCode::skipped_invalid_transform,
                       context + " has a non-finite, non-affine, or non-invertible transform; "
                                 "the node and descendants were skipped",
                       context);
    }
    return {};
}

[[nodiscard]] bool valid_child_pointer(const cgltf_data& data, const cgltf_node* child) noexcept {
    return child != nullptr && child >= data.nodes && child < data.nodes + data.nodes_count;
}

[[nodiscard]] Result<void> propagate_skipped_nodes(DocumentConstructionState& state,
                                                   std::vector<bool>& skipped_nodes) {
    std::vector<cgltf_size> stack;
    stack.reserve(state.data.nodes_count);
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        if (state.reachable[index] && skipped_nodes[index]) {
            stack.push_back(index);
        }
    }
    while (!stack.empty()) {
        const cgltf_node& parent = state.data.nodes[stack.back()];
        stack.pop_back();
        for (cgltf_size offset = 0; offset < parent.children_count; ++offset) {
            const cgltf_node* child = parent.children[offset];
            if (!valid_child_pointer(state.data, child)) {
                return Error{ErrorCode::scene_import_failed,
                             "A glTF node references a child outside the node table"};
            }
            const cgltf_size index = static_cast<cgltf_size>(child - state.data.nodes);
            if (!state.reachable[index] || skipped_nodes[index]) {
                continue;
            }
            skipped_nodes[index] = true;
            stack.push_back(index);
            const std::string context = node_context(*child, index);
            add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::scene,
                           ModelLoadDiagnosticCode::skipped_invalid_transform,
                           context + " was skipped because an ancestor transform was skipped",
                           context);
        }
    }
    return {};
}

[[nodiscard]] Result<std::vector<bool>>
used_document_meshes(const DocumentConstructionState& state,
                     const std::vector<bool>& skipped_nodes) {
    std::vector<bool> used(state.data.meshes_count, false);
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        const cgltf_node& node = state.data.nodes[index];
        if (!state.reachable[index] || skipped_nodes[index] || node.mesh == nullptr) {
            continue;
        }
        const std::size_t mesh_index = static_cast<std::size_t>(node.mesh - state.data.meshes);
        if (mesh_index >= used.size()) {
            return Error{ErrorCode::scene_import_failed,
                         "A glTF node references a mesh outside the mesh table"};
        }
        used[mesh_index] = true;
    }
    return used;
}

void prepare_imported_ids(DocumentConstructionState& state) {
    state.imported_ids.material_cache.resize(state.data.materials_count);
    state.imported_ids.materials.resize(state.data.materials_count);
    state.imported_ids.images.ids.resize(state.data.images_count);
    state.imported_ids.textures.resize(state.data.textures_count);
    state.imported_ids.samplers.resize(state.data.samplers_count);
    state.imported_ids.meshes.resize(state.data.meshes_count);
    state.imported_ids.primitives.resize(state.data.meshes_count);
}

[[nodiscard]] Result<void> import_document_meshes(DocumentConstructionState& state,
                                                  const std::vector<bool>& used_meshes) {
    prepare_imported_ids(state);
    ImportState import_state{
        state.data, state.gltf_path,  state.options, state.builder, state.imported_ids, {},
        {},         state.diagnostics};
    std::uint64_t primitive_count = 0;
    for (cgltf_size index = 0; index < state.data.meshes_count; ++index) {
        if (!used_meshes[index]) {
            continue;
        }
        Result<ImportedMesh> imported = import_mesh(import_state, state.data.meshes[index], index);
        if (!imported) {
            return imported.error();
        }
        primitive_count += imported.value().primitive_count;
        state.imported_ids.meshes[index] = imported.value().mesh;
        state.imported_ids.primitives[index] = std::move(imported).value().primitives;
    }
    if (primitive_count == 0) {
        return Error{ErrorCode::empty_scene_geometry,
                     "The glTF document contains no supported triangle geometry"};
    }
    return {};
}

[[nodiscard]] std::string imported_node_context(const cgltf_node& node, cgltf_size index) {
    const std::string label = node.name != nullptr ? std::string{node.name} : std::to_string(index);
    return "node " + label;
}

[[nodiscard]] Result<void> attach_perspective_camera(DocumentConstructionState& state,
                                                     const cgltf_node& node, NodeId node_id,
                                                     const std::string& context) {
    PerspectiveCameraDescription camera;
    camera.vertical_field_of_view_radians = node.camera->data.perspective.yfov;
    camera.near_plane = node.camera->data.perspective.znear;
    camera.far_plane =
        node.camera->data.perspective.has_zfar ? node.camera->data.perspective.zfar : 1.0e9F;
    const Result<void> attached = state.builder.set_node_perspective_camera(node_id, camera);
    if (!attached) {
        return Error{attached.error().code(), context + ": " + attached.error().message()};
    }
    if (!node.camera->data.perspective.has_zfar) {
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::camera,
                       ModelLoadDiagnosticCode::camera_fallback,
                       "Infinite-far perspective camera was bounded to 1e9 world units", context);
    }
    if (node.camera->data.perspective.has_aspect_ratio) {
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::camera,
                       ModelLoadDiagnosticCode::camera_fallback,
                       "Authored camera aspect ratio is not stored; the active viewport aspect "
                       "ratio is used",
                       context);
    }
    return {};
}

[[nodiscard]] Result<void> attach_imported_camera(DocumentConstructionState& state,
                                                  const cgltf_node& node, NodeId node_id,
                                                  cgltf_size node_index) {
    if (node.camera == nullptr) {
        return {};
    }
    const std::string context = imported_node_context(node, node_index);
    if (node.camera->type == cgltf_camera_type_perspective) {
        return attach_perspective_camera(state, node, node_id, context);
    }
    if (node.camera->type == cgltf_camera_type_orthographic) {
        add_diagnostic(state.diagnostics, ModelLoadDiagnosticCategory::camera,
                       ModelLoadDiagnosticCode::camera_fallback,
                       "Orthographic camera is not representable and was left as a "
                       "transform-only entity",
                       context);
    }
    return {};
}

[[nodiscard]] Result<void> create_document_nodes(DocumentConstructionState& state,
                                                 const std::vector<bool>& skipped_nodes,
                                                 const std::vector<Float4x4>& local_matrices) {
    state.imported_ids.nodes.resize(state.data.nodes_count);
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        if (!state.reachable[index] || skipped_nodes[index]) {
            continue;
        }
        const cgltf_node& node = state.data.nodes[index];
        Result<NodeId> created = state.builder.create_node(
            state.options.import_node_names && node.name != nullptr ? std::string_view{node.name}
                                                                    : std::string_view{});
        if (!created) {
            return created.error();
        }
        state.imported_ids.nodes[index] = created.value();
        const Result<void> matrix =
            state.builder.set_node_matrix(created.value(), local_matrices[index]);
        if (!matrix) {
            return Error{matrix.error().code(),
                         imported_node_context(node, index) + ": " + matrix.error().message()};
        }
        const Result<void> camera = attach_imported_camera(state, node, created.value(), index);
        if (!camera) {
            return camera.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> attach_imported_parent(DocumentConstructionState& state,
                                                  const cgltf_node& node, cgltf_size node_index) {
    if (node.parent == nullptr) {
        return {};
    }
    const std::size_t parent_index = static_cast<std::size_t>(node.parent - state.data.nodes);
    if (parent_index >= state.reachable.size() || !state.reachable[parent_index] ||
        !state.imported_ids.nodes[parent_index].has_value()) {
        return Error{ErrorCode::invalid_node_hierarchy,
                     "A glTF node has a parent outside the imported scenes"};
    }
    return state.builder.set_parent(state.imported_ids.nodes[node_index].value(),
                                    state.imported_ids.nodes[parent_index].value());
}

[[nodiscard]] Result<void> attach_imported_mesh(DocumentConstructionState& state,
                                                const cgltf_node& node, cgltf_size node_index) {
    if (node.mesh == nullptr) {
        return {};
    }
    const std::size_t mesh_index = static_cast<std::size_t>(node.mesh - state.data.meshes);
    if (mesh_index >= state.imported_ids.meshes.size()) {
        return Error{ErrorCode::scene_import_failed,
                     "A glTF node references a mesh outside the mesh table"};
    }
    if (!state.imported_ids.meshes[mesh_index].has_value()) {
        return {};
    }
    return state.builder.set_node_mesh(state.imported_ids.nodes[node_index].value(),
                                       *state.imported_ids.meshes[mesh_index]);
}

[[nodiscard]] Result<void> connect_document_nodes(DocumentConstructionState& state,
                                                  const std::vector<bool>& skipped_nodes) {
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        if (!state.reachable[index] || skipped_nodes[index]) {
            continue;
        }
        const cgltf_node& node = state.data.nodes[index];
        const Result<void> parent = attach_imported_parent(state, node, index);
        if (!parent) {
            return parent.error();
        }
        const Result<void> mesh = attach_imported_mesh(state, node, index);
        if (!mesh) {
            return mesh.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> add_imported_root(DocumentConstructionState& state,
                                             const std::vector<bool>& skipped_nodes,
                                             DocumentSceneId scene_id, const cgltf_node* root) {
    if (root == nullptr || root < state.data.nodes ||
        root >= state.data.nodes + state.data.nodes_count) {
        return Error{ErrorCode::invalid_node_hierarchy,
                     "A glTF scene contains an invalid root node"};
    }
    const std::size_t index = static_cast<std::size_t>(root - state.data.nodes);
    if (!state.reachable[index] || skipped_nodes[index] ||
        !state.imported_ids.nodes[index].has_value()) {
        return {};
    }
    return state.builder.add_scene_root(scene_id, *state.imported_ids.nodes[index]);
}

[[nodiscard]] Result<void> add_authored_scene_roots(DocumentConstructionState& state,
                                                    const std::vector<bool>& skipped_nodes) {
    for (cgltf_size scene_index = 0; scene_index < state.data.scenes_count; ++scene_index) {
        const cgltf_scene& scene = state.data.scenes[scene_index];
        for (cgltf_size root_index = 0; root_index < scene.nodes_count; ++root_index) {
            const Result<void> added =
                add_imported_root(state, skipped_nodes, state.imported_ids.scenes[scene_index],
                                  scene.nodes[root_index]);
            if (!added) {
                return added.error();
            }
        }
    }
    return {};
}

[[nodiscard]] bool is_implicit_scene_root(const DocumentConstructionState& state,
                                          const std::vector<bool>& skipped_nodes,
                                          cgltf_size index) {
    return state.reachable[index] && !skipped_nodes[index] &&
           state.data.nodes[index].parent == nullptr && state.imported_ids.nodes[index].has_value();
}

[[nodiscard]] Result<void> add_implicit_scene_roots(DocumentConstructionState& state,
                                                    const std::vector<bool>& skipped_nodes) {
    for (cgltf_size index = 0; index < state.data.nodes_count; ++index) {
        if (!is_implicit_scene_root(state, skipped_nodes, index)) {
            continue;
        }
        const Result<void> added = state.builder.add_scene_root(state.imported_ids.scenes[0],
                                                                *state.imported_ids.nodes[index]);
        if (!added) {
            return added.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> add_document_roots(DocumentConstructionState& state,
                                              const std::vector<bool>& skipped_nodes) {
    if (state.data.scenes_count != 0) {
        return add_authored_scene_roots(state, skipped_nodes);
    }
    return add_implicit_scene_roots(state, skipped_nodes);
}

[[nodiscard]] Result<Document> finish_imported_document(DocumentConstructionState& state) {
    Result<Document> document = state.builder.finish();
    if (!document) {
        return document.error();
    }
    Document value = std::move(document).value();
    const Result<void> metadata =
        attach_document_metadata(state.data, state.imported_ids, value, state.diagnostics);
    if (!metadata) {
        return metadata.error();
    }
    return value;
}

[[nodiscard]] Result<Document> construct_document_content(DocumentConstructionState& state) {
    std::vector<bool> skipped_nodes(state.data.nodes_count, false);
    std::vector<Float4x4> local_matrices(state.data.nodes_count);
    const Result<void> transforms = capture_node_transforms(state, skipped_nodes, local_matrices);
    if (!transforms) {
        return transforms.error();
    }
    const Result<void> propagated = propagate_skipped_nodes(state, skipped_nodes);
    if (!propagated) {
        return propagated.error();
    }
    Result<std::vector<bool>> used_meshes = used_document_meshes(state, skipped_nodes);
    if (!used_meshes) {
        return used_meshes.error();
    }
    const Result<void> meshes = import_document_meshes(state, used_meshes.value());
    if (!meshes) {
        return meshes.error();
    }
    const Result<void> nodes = create_document_nodes(state, skipped_nodes, local_matrices);
    if (!nodes) {
        return nodes.error();
    }
    const Result<void> connected = connect_document_nodes(state, skipped_nodes);
    if (!connected) {
        return connected.error();
    }
    const Result<void> roots = add_document_roots(state, skipped_nodes);
    if (!roots) {
        return roots.error();
    }
    return finish_imported_document(state);
}

[[nodiscard]] Result<ConstructedDocument>
construct_document(const cgltf_data& data, const std::vector<bool>& reachable,
                   const std::filesystem::path& gltf_path, const ModelLoadOptions& options,
                   std::vector<ModelLoadDiagnostic>& diagnostics) {
    try {
        DocumentConstructionState state{data, reachable, gltf_path, options, diagnostics};
        Result<DocumentSceneId> default_scene = create_document_scenes(state);
        if (!default_scene) {
            return default_scene.error();
        }
        Result<Document> document = construct_document_content(state);
        if (!document) {
            return document.error();
        }
        return ConstructedDocument{std::move(document).value(), default_scene.value()};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf::importer_detail

namespace elf3d::gltf {

using namespace importer_detail;

Result<LoadedDocument> load_document(const std::filesystem::path& path,
                                     const ModelLoadOptions& options) noexcept {
    try {
        Result<bool> is_glb = source_is_glb(path);
        if (!is_glb) {
            return is_glb.error();
        }
        DocumentLoadState state;
        state.is_glb = is_glb.value();
        if (const Result<void> parsed = parse_source(path, state); !parsed) {
            return parsed.error();
        }
        if (const Result<void> valid = validate_parsed_document(*state.data); !valid) {
            return valid.error();
        }
        if (const Result<void> buffers = load_external_buffers(path, state); !buffers) {
            return buffers.error();
        }
        Result<std::vector<bool>> reachable = reachable_nodes(*state.data);
        if (!reachable) {
            return reachable.error();
        }
        ModelLoadReport report =
            build_load_report(*state.data, reachable.value(), state.repaired_signed_buffer_fields);
        Result<ConstructedDocument> constructed =
            construct_document(*state.data, reachable.value(), path, options, report.diagnostics);
        if (!constructed) {
            return constructed.error();
        }
        ConstructedDocument value = std::move(constructed).value();
        return LoadedDocument{std::move(value.document), value.default_scene, std::move(report)};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf
