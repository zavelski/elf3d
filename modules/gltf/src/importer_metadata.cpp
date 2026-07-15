module;

#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/model/detail/imported_metadata.h>

#include <cgltf.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.model;

namespace elf3d::gltf::importer_metadata {

struct ImportedMetadataIds {
    std::span<const DocumentSceneId> scenes;
    std::span<const std::optional<NodeId>> nodes;
    std::span<const std::optional<MeshId>> meshes;
    std::span<const std::vector<std::optional<PrimitiveId>>> primitives;
    std::span<const std::vector<MaterialId>> materials;
    std::span<const std::optional<ImageId>> images;
    std::span<const std::optional<TextureId>> textures;
    std::span<const std::optional<SamplerId>> samplers;
};

namespace {

constexpr std::size_t maximum_preserved_json_block_bytes = 1024ULL * 1024ULL;
constexpr std::size_t maximum_preserved_json_bytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t maximum_preserved_extension_name_bytes = 256ULL;

struct MetadataBudget {
    std::size_t bytes = 0;
};

struct MetadataCopyState {
    MetadataBudget budget;
    model::detail::ImportedDocumentMetadata imported;
    std::vector<ModelLoadDiagnostic> warnings;

    [[nodiscard]] std::vector<ModelLoadDiagnostic>& diagnostics() noexcept {
        return warnings;
    }
};

[[nodiscard]] bool source_has_metadata(const cgltf_extras& extras,
                                       cgltf_size extensions_count) noexcept {
    return extras.data != nullptr || extras.end_offset > extras.start_offset ||
           extensions_count != 0;
}

void add_metadata_not_preserved(std::vector<ModelLoadDiagnostic>& diagnostics,
                                std::string_view context, std::string_view reason) {
    diagnostics.push_back(ModelLoadDiagnostic{
        ModelLoadDiagnosticSeverity::warning, ModelLoadDiagnosticCategory::metadata,
        ModelLoadDiagnosticCode::metadata_not_preserved,
        std::string{context} + " metadata was not preserved: " + std::string{reason},
        std::string{context}});
}

[[nodiscard]] bool reserve_metadata_bytes(MetadataBudget& budget, std::size_t bytes) noexcept {
    if (bytes > maximum_preserved_json_bytes - budget.bytes) {
        return false;
    }
    budget.bytes += bytes;
    return true;
}

[[nodiscard]] std::span<const cgltf_extension> extension_span(const cgltf_extension* extensions,
                                                              cgltf_size count) noexcept {
    return {extensions, static_cast<std::size_t>(count)};
}

[[nodiscard]] std::optional<std::string>
copy_extras_metadata(const cgltf_extras& extras, MetadataBudget& budget,
                     std::vector<ModelLoadDiagnostic>& diagnostics, std::string_view context) {
    if (extras.data == nullptr) {
        return std::nullopt;
    }
    const std::string_view data{extras.data};
    if (data.empty() || data.size() > maximum_preserved_json_block_bytes ||
        !reserve_metadata_bytes(budget, data.size())) {
        add_metadata_not_preserved(diagnostics, context,
                                   "extras exceed the bounded raw-JSON budget");
        return std::nullopt;
    }
    return std::string{data};
}

[[nodiscard]] bool extension_is_duplicate(std::span<const ModelJsonExtension> extensions,
                                          std::string_view name) noexcept {
    return std::any_of(
        extensions.begin(), extensions.end(),
        [name](const ModelJsonExtension& preserved) noexcept { return preserved.name == name; });
}

[[nodiscard]] std::optional<ModelJsonExtension>
copy_extension(const cgltf_extension& extension,
               std::span<const ModelJsonExtension> preserved_extensions, MetadataBudget& budget,
               std::vector<ModelLoadDiagnostic>& diagnostics, std::string_view context) {
    if (extension.name == nullptr || extension.data == nullptr) {
        add_metadata_not_preserved(diagnostics, context,
                                   "an unknown extension has missing raw data");
        return std::nullopt;
    }
    const std::string_view name{extension.name};
    const std::string_view data{extension.data};
    const bool duplicate = extension_is_duplicate(preserved_extensions, name);
    if (duplicate || name.empty() || name.size() > maximum_preserved_extension_name_bytes ||
        data.empty() || data.size() > maximum_preserved_json_block_bytes ||
        !reserve_metadata_bytes(budget, name.size() + data.size())) {
        add_metadata_not_preserved(
            diagnostics, context,
            duplicate ? "an unknown extension name is duplicated"
                      : "an unknown extension exceeds the bounded raw-JSON budget");
        return std::nullopt;
    }
    return ModelJsonExtension{std::string{name}, std::string{data}};
}

[[nodiscard]] ModelJsonMetadata copy_metadata(const cgltf_extras& extras,
                                              std::span<const cgltf_extension> extensions,
                                              MetadataBudget& budget,
                                              std::vector<ModelLoadDiagnostic>& diagnostics,
                                              std::string_view context) {
    ModelJsonMetadata result;
    if (std::optional<std::string> extras_json =
            copy_extras_metadata(extras, budget, diagnostics, context);
        extras_json.has_value()) {
        result.extras_json = std::move(extras_json).value();
    }
    result.extensions.reserve(extensions.size());
    for (const cgltf_extension& extension : extensions) {
        std::optional<ModelJsonExtension> copied =
            copy_extension(extension, result.extensions, budget, diagnostics, context);
        if (copied.has_value()) {
            result.extensions.push_back(std::move(copied).value());
        }
    }
    return result;
}

[[nodiscard]] std::optional<std::string>
copy_legacy_extras(const cgltf_data& source, const cgltf_extras& extras, MetadataBudget& budget,
                   std::vector<ModelLoadDiagnostic>& diagnostics, std::string_view context) {
    if (extras.data != nullptr || extras.end_offset <= extras.start_offset) {
        return std::nullopt;
    }
    if (source.json == nullptr || extras.end_offset > source.json_size) {
        add_metadata_not_preserved(diagnostics, context,
                                   "legacy raw extras offsets are outside the source JSON");
        return std::nullopt;
    }
    const std::string_view data{source.json + extras.start_offset,
                                extras.end_offset - extras.start_offset};
    if (data.empty() || data.size() > maximum_preserved_json_block_bytes ||
        !reserve_metadata_bytes(budget, data.size())) {
        add_metadata_not_preserved(diagnostics, context,
                                   "extras exceed the bounded raw-JSON budget");
        return std::nullopt;
    }
    return std::string{data};
}

[[nodiscard]] ModelJsonMetadata copy_mesh_metadata(const cgltf_data& data, const cgltf_mesh& mesh,
                                                   MetadataBudget& budget,
                                                   std::vector<ModelLoadDiagnostic>& diagnostics,
                                                   std::string_view context) {
    ModelJsonMetadata metadata =
        copy_metadata(mesh.extras, extension_span(mesh.extensions, mesh.extensions_count), budget,
                      diagnostics, context);
    if (std::optional<std::string> legacy =
            copy_legacy_extras(data, mesh.extras, budget, diagnostics, context);
        legacy.has_value()) {
        metadata.extras_json = std::move(legacy).value();
    }
    return metadata;
}

[[nodiscard]] bool has_buffer_metadata(const cgltf_data& data) noexcept {
    for (cgltf_size index = 0; index < data.buffers_count; ++index) {
        if (source_has_metadata(data.buffers[index].extras, data.buffers[index].extensions_count)) {
            return true;
        }
    }
    for (cgltf_size index = 0; index < data.buffer_views_count; ++index) {
        if (source_has_metadata(data.buffer_views[index].extras,
                                data.buffer_views[index].extensions_count)) {
            return true;
        }
    }
    for (cgltf_size index = 0; index < data.accessors_count; ++index) {
        if (source_has_metadata(data.accessors[index].extras,
                                data.accessors[index].extensions_count)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_camera_metadata(const cgltf_data& data) noexcept {
    for (cgltf_size index = 0; index < data.cameras_count; ++index) {
        const cgltf_camera& camera = data.cameras[index];
        const cgltf_extras& projection_extras = camera.type == cgltf_camera_type_perspective
                                                    ? camera.data.perspective.extras
                                                    : camera.data.orthographic.extras;
        if (source_has_metadata(camera.extras, camera.extensions_count) ||
            source_has_metadata(projection_extras, 0)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_animation_metadata(const cgltf_data& data) noexcept {
    for (cgltf_size index = 0; index < data.animations_count; ++index) {
        const cgltf_animation& animation = data.animations[index];
        if (source_has_metadata(animation.extras, animation.extensions_count)) {
            return true;
        }
        for (cgltf_size sampler = 0; sampler < animation.samplers_count; ++sampler) {
            if (source_has_metadata(animation.samplers[sampler].extras,
                                    animation.samplers[sampler].extensions_count)) {
                return true;
            }
        }
        for (cgltf_size channel = 0; channel < animation.channels_count; ++channel) {
            if (source_has_metadata(animation.channels[channel].extras,
                                    animation.channels[channel].extensions_count)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool has_auxiliary_object_metadata(const cgltf_data& data) noexcept {
    for (cgltf_size index = 0; index < data.skins_count; ++index) {
        if (source_has_metadata(data.skins[index].extras, data.skins[index].extensions_count)) {
            return true;
        }
    }
    for (cgltf_size index = 0; index < data.lights_count; ++index) {
        if (source_has_metadata(data.lights[index].extras, 0)) {
            return true;
        }
    }
    for (cgltf_size index = 0; index < data.variants_count; ++index) {
        if (source_has_metadata(data.variants[index].extras, 0)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_mapping_metadata(const cgltf_data& data) noexcept {
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            for (cgltf_size mapping = 0; mapping < primitive.mappings_count; ++mapping) {
                if (source_has_metadata(primitive.mappings[mapping].extras, 0)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void diagnose_unsupported_scope_metadata(const cgltf_data& data,
                                         std::vector<ModelLoadDiagnostic>& diagnostics) {
    if (!has_buffer_metadata(data) && !has_camera_metadata(data) && !has_animation_metadata(data) &&
        !has_auxiliary_object_metadata(data) && !has_mapping_metadata(data)) {
        return;
    }
    add_metadata_not_preserved(
        diagnostics, "glTF auxiliary scopes",
        "buffers, buffer views, accessors, cameras, skins, and animations have no stable "
        "one-to-one Document metadata identity");
}

[[nodiscard]] bool has_preserved_values(const ModelJsonMetadata& metadata) noexcept {
    return metadata.extras_json.has_value() || !metadata.extensions.empty();
}

template <typename Container, typename Id>
void store_preserved_metadata(Container& destination, Id id, ModelJsonMetadata metadata) {
    if (has_preserved_values(metadata)) {
        destination.emplace_back(id, std::move(metadata));
    }
}

void copy_root_metadata(const cgltf_data& data, MetadataCopyState& state) {
    state.imported.root =
        copy_metadata(data.extras, extension_span(data.data_extensions, data.data_extensions_count),
                      state.budget, state.diagnostics(), "document root");
    state.imported.asset = copy_metadata(
        data.asset.extras, extension_span(data.asset.extensions, data.asset.extensions_count),
        state.budget, state.diagnostics(), "asset");
}

void copy_scene_metadata(const cgltf_data& data, const ImportedMetadataIds& ids,
                         MetadataCopyState& state) {
    for (cgltf_size index = 0; index < data.scenes_count; ++index) {
        const cgltf_scene& source = data.scenes[index];
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, extension_span(source.extensions, source.extensions_count),
                          state.budget, state.diagnostics(), "scene " + std::to_string(index));
        store_preserved_metadata(state.imported.scenes, ids.scenes[index], std::move(metadata));
    }
}

void copy_node_metadata(const cgltf_data& data, const ImportedMetadataIds& ids,
                        MetadataCopyState& state) {
    for (cgltf_size index = 0; index < data.nodes_count; ++index) {
        const cgltf_node& source = data.nodes[index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        if (!ids.nodes[index].has_value()) {
            add_metadata_not_preserved(state.diagnostics(), "node " + std::to_string(index),
                                       "the node was not imported");
            continue;
        }
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, extension_span(source.extensions, source.extensions_count),
                          state.budget, state.diagnostics(), "node " + std::to_string(index));
        store_preserved_metadata(state.imported.nodes, *ids.nodes[index], std::move(metadata));
    }
}

void copy_primitive_metadata(const cgltf_mesh& mesh, std::size_t mesh_index,
                             const ImportedMetadataIds& ids, MetadataCopyState& state) {
    for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
         ++primitive_index) {
        const cgltf_primitive& source = mesh.primitives[primitive_index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        std::optional<PrimitiveId> id;
        if (mesh_index < ids.primitives.size() &&
            primitive_index < ids.primitives[mesh_index].size()) {
            id = ids.primitives[mesh_index][primitive_index];
        }
        const std::string context =
            "mesh " + std::to_string(mesh_index) + ", primitive " + std::to_string(primitive_index);
        if (!id.has_value()) {
            add_metadata_not_preserved(state.diagnostics(), context,
                                       "the primitive was not imported");
            continue;
        }
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, extension_span(source.extensions, source.extensions_count),
                          state.budget, state.diagnostics(), context);
        store_preserved_metadata(state.imported.primitives, *id, std::move(metadata));
    }
}

void copy_mesh_metadata(const cgltf_data& data, const ImportedMetadataIds& ids,
                        MetadataCopyState& state) {
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& source = data.meshes[mesh_index];
        if (source_has_metadata(source.extras, source.extensions_count)) {
            if (!ids.meshes[mesh_index].has_value()) {
                add_metadata_not_preserved(state.diagnostics(),
                                           "mesh " + std::to_string(mesh_index),
                                           "the mesh was not imported");
            } else {
                const std::string context = "mesh " + std::to_string(mesh_index);
                ModelJsonMetadata metadata =
                    copy_mesh_metadata(data, source, state.budget, state.diagnostics(), context);
                store_preserved_metadata(state.imported.meshes, *ids.meshes[mesh_index],
                                         std::move(metadata));
            }
        }
        copy_primitive_metadata(source, static_cast<std::size_t>(mesh_index), ids, state);
    }
}

void copy_material_metadata(const cgltf_data& data, const ImportedMetadataIds& ids,
                            MetadataCopyState& state) {
    for (cgltf_size index = 0; index < data.materials_count; ++index) {
        const cgltf_material& source = data.materials[index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        if (ids.materials[index].empty()) {
            add_metadata_not_preserved(state.diagnostics(), "material " + std::to_string(index),
                                       "the material was not imported");
            continue;
        }
        for (const MaterialId id : ids.materials[index]) {
            ModelJsonMetadata metadata = copy_metadata(
                source.extras, extension_span(source.extensions, source.extensions_count),
                state.budget, state.diagnostics(), "material " + std::to_string(index));
            store_preserved_metadata(state.imported.materials, id, std::move(metadata));
        }
    }
}

template <typename Source, typename Id, typename Destination>
void copy_indexed_metadata(std::span<const Source> sources, std::span<const std::optional<Id>> ids,
                           Destination& destination, std::string_view label,
                           MetadataCopyState& state) {
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const Source& source = sources[index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        const std::string context = std::string{label} + " " + std::to_string(index);
        if (!ids[index].has_value()) {
            add_metadata_not_preserved(state.diagnostics(), context, "the object was not imported");
            continue;
        }
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, extension_span(source.extensions, source.extensions_count),
                          state.budget, state.diagnostics(), context);
        store_preserved_metadata(destination, *ids[index], std::move(metadata));
    }
}

void append_metadata_diagnostics(std::vector<ModelLoadDiagnostic>& destination,
                                 std::vector<ModelLoadDiagnostic>& source) {
    for (ModelLoadDiagnostic& diagnostic : source) {
        destination.push_back(std::move(diagnostic));
    }
}

} // namespace

Result<void> attach_imported_metadata(const cgltf_data& data, const ImportedMetadataIds& ids,
                                      Document& document,
                                      std::vector<ModelLoadDiagnostic>& diagnostics) {
    MetadataCopyState state;
    copy_root_metadata(data, state);
    copy_scene_metadata(data, ids, state);
    copy_node_metadata(data, ids, state);
    copy_mesh_metadata(data, ids, state);
    copy_material_metadata(data, ids, state);
    copy_indexed_metadata(
        std::span<const cgltf_image>{data.images, static_cast<std::size_t>(data.images_count)},
        ids.images, state.imported.images, "image", state);
    copy_indexed_metadata(std::span<const cgltf_texture>{data.textures, static_cast<std::size_t>(
                                                                            data.textures_count)},
                          ids.textures, state.imported.textures, "texture", state);
    copy_indexed_metadata(std::span<const cgltf_sampler>{data.samplers, static_cast<std::size_t>(
                                                                            data.samplers_count)},
                          ids.samplers, state.imported.samplers, "sampler", state);
    diagnose_unsupported_scope_metadata(data, state.diagnostics());
    append_metadata_diagnostics(diagnostics, state.warnings);
    return model::detail::DocumentMetadataAccess::attach_import_metadata(document,
                                                                         std::move(state.imported));
}

} // namespace elf3d::gltf::importer_metadata
