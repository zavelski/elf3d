module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "exporter_internal.hpp"
#include "exporter_json_internal.hpp"

#include <algorithm>
#include <limits>

module elf.gltf;

namespace elf3d::gltf::exporter_detail {
namespace {

template <typename Id>
[[nodiscard]] std::optional<std::uint32_t> find_index(const std::vector<IdIndex<Id>>& indices,
                                                      Id id) noexcept {
    for (const IdIndex<Id>& entry : indices) {
        if (entry.id == id) {
            return entry.index;
        }
    }
    return std::nullopt;
}

struct GeneratedExtensions {
    bool texture_transform = false;
    bool unlit = false;
    bool ior = false;
    bool specular = false;
    bool emissive_strength = false;
};

[[nodiscard]] bool uses_texture_transform(const ModelMaterialDescription& material) noexcept {
    return has_nondefault_transform(material.base_color_texture_mapping) ||
           has_nondefault_transform(material.metallic_roughness_texture_mapping) ||
           has_nondefault_transform(material.normal_texture_mapping) ||
           has_nondefault_transform(material.occlusion_texture_mapping) ||
           has_nondefault_transform(material.emissive_texture_mapping);
}

void update_generated_extensions(GeneratedExtensions& extensions,
                                 const ModelMaterialDescription& material) noexcept {
    extensions.texture_transform = extensions.texture_transform || uses_texture_transform(material);
    extensions.unlit = extensions.unlit || material.unlit;
    extensions.ior = extensions.ior || material.ior != 1.5F;
    extensions.specular = extensions.specular || material.specular_factor != 1.0F ||
                          material.specular_color_factor != Float3{1.0F, 1.0F, 1.0F};
    extensions.emissive_strength =
        extensions.emissive_strength || material.emissive_strength != 1.0F;
}

void add_generated_extensions(std::vector<std::string_view>& destination,
                              const ExportData& document) {
    GeneratedExtensions generated;
    for (const MaterialView& material : document.materials) {
        update_generated_extensions(generated, material.description);
    }
    if (generated.texture_transform) {
        add_extension_used(destination, "KHR_texture_transform");
    }
    if (generated.unlit) {
        add_extension_used(destination, "KHR_materials_unlit");
    }
    if (generated.ior) {
        add_extension_used(destination, "KHR_materials_ior");
    }
    if (generated.specular) {
        add_extension_used(destination, "KHR_materials_specular");
    }
    if (generated.emissive_strength) {
        add_extension_used(destination, "KHR_materials_emissive_strength");
    }
}

void add_object_extensions(std::vector<std::string_view>& destination, const ExportData& document) {
    for (const DocumentSceneView& value : document.scenes) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const NodeView& value : document.nodes) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const MeshView& value : document.meshes) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const PrimitiveView& value : document.primitives) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const MaterialView& value : document.materials) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const ImageView& value : document.images) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const TextureView& value : document.textures) {
        add_preserved_extensions_used(destination, value.metadata);
    }
    for (const SamplerView& value : document.samplers) {
        add_preserved_extensions_used(destination, value.metadata);
    }
}

void append_extensions_used(std::string& output, const ExportData& document) {
    std::vector<std::string_view> extensions;
    add_generated_extensions(extensions, document);
    add_preserved_extensions_used(extensions, document.root_metadata);
    add_preserved_extensions_used(extensions, document.asset_metadata);
    add_object_extensions(extensions, document);
    if (extensions.empty()) {
        return;
    }
    output.append(",\"extensionsUsed\":[");
    for (std::size_t index = 0; index < extensions.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        append_string(output, extensions[index]);
    }
    output.push_back(']');
}

[[nodiscard]] Result<std::optional<std::uint32_t>> default_scene_index(const ExportData& document) {
    if (!document.default_scene.has_value()) {
        return std::optional<std::uint32_t>{};
    }
    const auto scene = std::find_if(document.scenes.begin(), document.scenes.end(),
                                    [&document](const DocumentSceneView& value) noexcept {
                                        return value.id == *document.default_scene;
                                    });
    if (scene == document.scenes.end()) {
        return Error{ErrorCode::invalid_argument,
                     "Document default scene is not in the scene table"};
    }
    const std::size_t index = static_cast<std::size_t>(scene - document.scenes.begin());
    if (index > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit index limit"};
    }
    return std::optional<std::uint32_t>{static_cast<std::uint32_t>(index)};
}

[[nodiscard]] Result<void> append_scene_roots(std::string& output, const ExportData& document,
                                              const DocumentSceneView& scene, bool& has_member) {
    if (scene.roots.empty()) {
        return {};
    }
    if (has_member) {
        output.push_back(',');
    }
    output.append("\"nodes\":[");
    for (std::size_t index = 0; index < scene.roots.size(); ++index) {
        const std::optional<std::uint32_t> node =
            find_index(document.node_indices, scene.roots[index]);
        if (!node) {
            return Error{ErrorCode::invalid_argument, "Scene root is not in this document"};
        }
        if (index != 0U) {
            output.push_back(',');
        }
        append_unsigned(output, *node);
    }
    output.push_back(']');
    has_member = true;
    return {};
}

[[nodiscard]] Result<void> append_scene(std::string& output, const ExportData& document,
                                        const DocumentSceneView& scene) {
    output.push_back('{');
    bool has_member = false;
    if (!scene.name.empty()) {
        append_string(output, "name");
        output.push_back(':');
        append_string(output, scene.name);
        has_member = true;
    }
    if (const Result<void> roots = append_scene_roots(output, document, scene, has_member);
        !roots) {
        return roots.error();
    }
    append_preserved_metadata(output, scene.metadata, has_member);
    output.push_back('}');
    return {};
}

[[nodiscard]] Result<void> append_scenes(std::string& output, const ExportData& document) {
    if (document.scenes.empty()) {
        return {};
    }
    const Result<std::optional<std::uint32_t>> selected = default_scene_index(document);
    if (!selected) {
        return selected.error();
    }
    if (selected.value().has_value()) {
        output.append(",\"scene\":");
        append_unsigned(output, *selected.value());
    }
    output.append(",\"scenes\":[");
    for (std::size_t index = 0; index < document.scenes.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> scene = append_scene(output, document, document.scenes[index]);
            !scene) {
            return scene.error();
        }
    }
    output.push_back(']');
    return {};
}

void append_node_matrix(std::string& output, const NodeView& node) {
    output.append("\"matrix\":[");
    for (std::size_t index = 0; index < node.local_matrix.elements.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        append_float(output, node.local_matrix.elements[index]);
    }
    output.push_back(']');
}

[[nodiscard]] Result<void> append_node_children(std::string& output, const ExportData& document,
                                                const NodeView& node) {
    if (node.children.empty()) {
        return {};
    }
    output.append(",\"children\":[");
    for (std::size_t index = 0; index < node.children.size(); ++index) {
        const std::optional<std::uint32_t> child =
            find_index(document.node_indices, node.children[index]);
        if (!child) {
            return Error{ErrorCode::invalid_argument, "Node child is not in this document"};
        }
        if (index != 0U) {
            output.push_back(',');
        }
        append_unsigned(output, *child);
    }
    output.push_back(']');
    return {};
}

[[nodiscard]] Result<void> append_node(std::string& output, const ExportData& document,
                                       const NodeView& node, std::uint32_t& camera) {
    output.push_back('{');
    bool first = true;
    const auto separator = [&output, &first]() {
        if (!first) {
            output.push_back(',');
        }
        first = false;
    };
    if (!node.name.empty()) {
        separator();
        append_string(output, "name");
        output.push_back(':');
        append_string(output, node.name);
    }
    if (node.mesh) {
        const std::optional<std::uint32_t> mesh = find_index(document.mesh_indices, *node.mesh);
        if (!mesh) {
            return Error{ErrorCode::invalid_argument, "Node mesh is not in this document"};
        }
        separator();
        output.append("\"mesh\":");
        append_unsigned(output, *mesh);
    }
    if (node.perspective_camera) {
        separator();
        output.append("\"camera\":");
        append_unsigned(output, camera++);
    }
    separator();
    append_node_matrix(output, node);
    if (const Result<void> children = append_node_children(output, document, node); !children) {
        return children.error();
    }
    append_preserved_metadata(output, node.metadata);
    output.push_back('}');
    return {};
}

[[nodiscard]] Result<void> append_nodes(std::string& output, const ExportData& document) {
    if (document.nodes.empty()) {
        return {};
    }
    output.append(",\"nodes\":[");
    std::uint32_t camera = 0;
    for (std::size_t index = 0; index < document.nodes.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> node = append_node(output, document, document.nodes[index], camera);
            !node) {
            return node.error();
        }
    }
    output.push_back(']');
    return {};
}

void append_cameras(std::string& output, const ExportData& document) {
    const bool has_cameras =
        std::any_of(document.nodes.begin(), document.nodes.end(),
                    [](const NodeView& node) { return node.perspective_camera.has_value(); });
    if (!has_cameras) {
        return;
    }
    output.append(",\"cameras\":[");
    bool first = true;
    for (const NodeView& node : document.nodes) {
        if (!node.perspective_camera) {
            continue;
        }
        if (!first) {
            output.push_back(',');
        }
        first = false;
        const ModelPerspectiveCameraDescription& camera = *node.perspective_camera;
        output.append("{\"type\":\"perspective\",\"perspective\":{\"yfov\":");
        append_float(output, camera.vertical_field_of_view_radians);
        output.append(",\"znear\":");
        append_float(output, camera.near_plane);
        output.append(",\"zfar\":");
        append_float(output, camera.far_plane);
        output.append("}}");
    }
    output.push_back(']');
}

void append_primitive_attributes(std::string& output, const PrimitiveOutput& primitive) {
    output.append("{\"attributes\":{\"POSITION\":");
    append_unsigned(output, primitive.positions);
    const auto optional_attribute = [&output](std::string_view name,
                                              std::optional<std::uint32_t> value) {
        if (value) {
            output.append(",\"");
            output.append(name);
            output.append("\":");
            append_unsigned(output, *value);
        }
    };
    optional_attribute("NORMAL", primitive.normals);
    optional_attribute("TEXCOORD_0", primitive.texcoord0);
    optional_attribute("TEXCOORD_1", primitive.texcoord1);
    optional_attribute("COLOR_0", primitive.colors);
    output.append("},\"indices\":");
    append_unsigned(output, primitive.indices);
    output.append(",\"material\":");
    append_unsigned(output, primitive.material);
    output.append(",\"mode\":4");
}

[[nodiscard]] Result<void> append_mesh(std::string& output, const ExportPackage& package,
                                       const MeshView& mesh) {
    output.push_back('{');
    if (!mesh.name.empty()) {
        append_string(output, "name");
        output.push_back(':');
        append_string(output, mesh.name);
        output.push_back(',');
    }
    output.append("\"primitives\":[");
    for (std::size_t index = 0; index < mesh.primitives.size(); ++index) {
        const std::optional<std::uint32_t> primitive =
            find_index(package.document.primitive_indices, mesh.primitives[index]);
        if (!primitive || *primitive >= package.primitives.size()) {
            return Error{ErrorCode::invalid_argument, "Mesh primitive is not in this document"};
        }
        if (index != 0U) {
            output.push_back(',');
        }
        append_primitive_attributes(output, package.primitives[*primitive]);
        append_preserved_metadata(output, package.document.primitives[*primitive].metadata);
        output.push_back('}');
    }
    output.push_back(']');
    append_preserved_metadata(output, mesh.metadata);
    output.push_back('}');
    return {};
}

[[nodiscard]] Result<void> append_meshes(std::string& output, const ExportPackage& package) {
    if (package.document.meshes.empty()) {
        return {};
    }
    output.append(",\"meshes\":[");
    for (std::size_t index = 0; index < package.document.meshes.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> mesh = append_mesh(output, package, package.document.meshes[index]);
            !mesh) {
            return mesh.error();
        }
    }
    output.push_back(']');
    return {};
}

struct MaterialTextures {
    std::optional<std::uint32_t> base;
    std::optional<std::uint32_t> metallic_roughness;
    std::optional<std::uint32_t> normal;
    std::optional<std::uint32_t> occlusion;
    std::optional<std::uint32_t> emissive;
};

[[nodiscard]] Result<std::optional<std::uint32_t>> material_texture(const ExportData& document,
                                                                    TextureId id) {
    if (!id.is_valid()) {
        return std::optional<std::uint32_t>{};
    }
    const std::optional<std::uint32_t> found = find_index(document.texture_indices, id);
    if (!found) {
        return Error{ErrorCode::invalid_argument, "Material texture is not in this document"};
    }
    return found;
}

[[nodiscard]] Result<MaterialTextures>
collect_material_textures(const ExportData& document, const ModelMaterialDescription& material) {
    const Result<std::optional<std::uint32_t>> base =
        material_texture(document, material.base_color_texture);
    const Result<std::optional<std::uint32_t>> metallic =
        material_texture(document, material.metallic_roughness_texture);
    const Result<std::optional<std::uint32_t>> normal =
        material_texture(document, material.normal_texture);
    const Result<std::optional<std::uint32_t>> occlusion =
        material_texture(document, material.occlusion_texture);
    const Result<std::optional<std::uint32_t>> emissive =
        material_texture(document, material.emissive_texture);
    if (!base || !metallic || !normal || !occlusion || !emissive) {
        return Error{ErrorCode::invalid_argument, "Material texture is not in this document"};
    }
    return MaterialTextures{base.value(), metallic.value(), normal.value(), occlusion.value(),
                            emissive.value()};
}

void append_material_pbr(std::string& output, const ModelMaterialDescription& material,
                         const MaterialTextures& textures) {
    output.append("{\"pbrMetallicRoughness\":{\"baseColorFactor\":");
    append_float4(output, material.base_color);
    output.append(",\"metallicFactor\":");
    append_float(output, material.metallic_factor);
    output.append(",\"roughnessFactor\":");
    append_float(output, material.roughness_factor);
    if (textures.base) {
        output.append(",\"baseColorTexture\":");
        append_texture_info(output, *textures.base, material.base_color_texture_mapping);
    }
    if (textures.metallic_roughness) {
        output.append(",\"metallicRoughnessTexture\":");
        append_texture_info(output, *textures.metallic_roughness,
                            material.metallic_roughness_texture_mapping);
    }
    output.push_back('}');
}

void append_material_texture_slots(std::string& output, const ModelMaterialDescription& material,
                                   const MaterialTextures& textures) {
    if (textures.normal) {
        output.append(",\"normalTexture\":");
        append_texture_info(output, *textures.normal, material.normal_texture_mapping,
                            std::pair<std::string_view, float>{"scale", material.normal_scale});
    }
    if (textures.occlusion) {
        output.append(",\"occlusionTexture\":");
        append_texture_info(
            output, *textures.occlusion, material.occlusion_texture_mapping,
            std::pair<std::string_view, float>{"strength", material.occlusion_strength});
    }
    if (textures.emissive) {
        output.append(",\"emissiveTexture\":");
        append_texture_info(output, *textures.emissive, material.emissive_texture_mapping);
    }
}

void append_material_properties(std::string& output, const ModelMaterialDescription& material) {
    output.append(",\"emissiveFactor\":");
    append_float3(output, material.emissive_factor);
    if (material.alpha_mode != ModelAlphaMode::opaque) {
        output.append(",\"alphaMode\":");
        append_string(output, material.alpha_mode == ModelAlphaMode::mask ? "MASK" : "BLEND");
    }
    if (material.alpha_mode == ModelAlphaMode::mask) {
        output.append(",\"alphaCutoff\":");
        append_float(output, material.alpha_cutoff);
    }
    if (material.double_sided) {
        output.append(",\"doubleSided\":true");
    }
}

void append_standard_material_extensions(std::string& output, bool& first,
                                         std::vector<std::string_view>& emitted,
                                         const ModelMaterialDescription& material) {
    if (material.unlit && begin_extension_member(output, first, emitted, "KHR_materials_unlit")) {
        output.append("{}");
    }
    if (material.ior != 1.5F &&
        begin_extension_member(output, first, emitted, "KHR_materials_ior")) {
        output.append("{\"ior\":");
        append_float(output, material.ior);
        output.push_back('}');
    }
    if (material.emissive_strength != 1.0F &&
        begin_extension_member(output, first, emitted, "KHR_materials_emissive_strength")) {
        output.append("{\"emissiveStrength\":");
        append_float(output, material.emissive_strength);
        output.push_back('}');
    }
}

void append_specular_extension(std::string& output, bool& first,
                               std::vector<std::string_view>& emitted,
                               const ModelMaterialDescription& material) {
    if (material.specular_factor == 1.0F &&
        material.specular_color_factor == Float3{1.0F, 1.0F, 1.0F}) {
        return;
    }
    if (!begin_extension_member(output, first, emitted, "KHR_materials_specular")) {
        return;
    }
    output.append("{\"specularFactor\":");
    append_float(output, material.specular_factor);
    output.append(",\"specularColorFactor\":");
    append_float3(output, material.specular_color_factor);
    output.push_back('}');
}

void append_material_extensions(std::string& output, const ModelMaterialDescription& material,
                                ModelJsonMetadataView metadata) {
    const bool required = material.unlit || material.ior != 1.5F ||
                          material.emissive_strength != 1.0F || material.specular_factor != 1.0F ||
                          material.specular_color_factor != Float3{1.0F, 1.0F, 1.0F} ||
                          !metadata.extensions.empty();
    if (!required) {
        return;
    }
    output.append(",\"extensions\":{");
    bool first = true;
    std::vector<std::string_view> emitted;
    emitted.reserve(metadata.extensions.size() + 4U);
    append_standard_material_extensions(output, first, emitted, material);
    append_specular_extension(output, first, emitted, material);
    append_preserved_extension_members(output, first, emitted, metadata.extensions);
    output.push_back('}');
}

[[nodiscard]] Result<void> append_material(std::string& output, const ExportData& document,
                                           const MaterialView& view) {
    const ModelMaterialDescription& material = view.description;
    const Result<MaterialTextures> textures = collect_material_textures(document, material);
    if (!textures) {
        return textures.error();
    }
    append_material_pbr(output, material, textures.value());
    append_material_texture_slots(output, material, textures.value());
    append_material_properties(output, material);
    append_material_extensions(output, material, view.metadata);
    append_preserved_extras(output, view.metadata);
    output.push_back('}');
    return {};
}

[[nodiscard]] Result<void> append_materials(std::string& output, const ExportData& document) {
    if (document.materials.empty()) {
        return {};
    }
    output.append(",\"materials\":[");
    for (std::size_t index = 0; index < document.materials.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> material =
                append_material(output, document, document.materials[index]);
            !material) {
            return material.error();
        }
    }
    output.push_back(']');
    return {};
}

[[nodiscard]] Result<void> append_textures(std::string& output, const ExportData& document) {
    if (document.textures.empty()) {
        return {};
    }
    output.append(",\"textures\":[");
    for (std::size_t index = 0; index < document.textures.size(); ++index) {
        const ModelTextureDescription& texture = document.textures[index].description;
        const std::optional<std::uint32_t> image =
            find_index(document.image_indices, texture.image);
        const std::optional<std::uint32_t> sampler =
            find_index(document.sampler_indices, texture.sampler);
        if (!image || !sampler) {
            return Error{ErrorCode::invalid_argument, "Texture source is not in this document"};
        }
        if (index != 0U) {
            output.push_back(',');
        }
        output.append("{\"sampler\":");
        append_unsigned(output, *sampler);
        output.append(",\"source\":");
        append_unsigned(output, *image);
        append_preserved_metadata(output, document.textures[index].metadata);
        output.push_back('}');
    }
    output.push_back(']');
    return {};
}

[[nodiscard]] Result<void> append_image(std::string& output, const ExportPackage& package,
                                        std::size_t index) {
    if (package.image_views[index]) {
        if (index >= package.image_mime_types.size()) {
            return Error{ErrorCode::invalid_argument, "Embedded image output MIME type is missing"};
        }
        output.append("{\"bufferView\":");
        append_unsigned(output, *package.image_views[index]);
        output.append(",\"mimeType\":");
        append_string(output, image_mime_text(package.image_mime_types[index]));
    } else {
        if (index >= package.image_uris.size()) {
            return Error{ErrorCode::invalid_argument, "External image output URI is missing"};
        }
        output.append("{\"uri\":");
        append_string(output, package.image_uris[index]);
    }
    append_preserved_metadata(output, package.document.images[index].metadata);
    output.push_back('}');
    return {};
}

[[nodiscard]] Result<void> append_images(std::string& output, const ExportPackage& package) {
    if (package.document.images.empty()) {
        return {};
    }
    output.append(",\"images\":[");
    for (std::size_t index = 0; index < package.document.images.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> image = append_image(output, package, index); !image) {
            return image.error();
        }
    }
    output.push_back(']');
    return {};
}

void append_samplers(std::string& output, const ExportData& document) {
    if (document.samplers.empty()) {
        return;
    }
    output.append(",\"samplers\":[");
    for (std::size_t index = 0; index < document.samplers.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        const ModelSamplerDescription& sampler = document.samplers[index].description;
        output.append("{\"magFilter\":");
        append_unsigned(output, filter_value(sampler.mag_filter));
        output.append(",\"minFilter\":");
        append_unsigned(output, filter_value(sampler.min_filter));
        output.append(",\"wrapS\":");
        append_unsigned(output, wrap_value(sampler.wrap_u));
        output.append(",\"wrapT\":");
        append_unsigned(output, wrap_value(sampler.wrap_v));
        append_preserved_metadata(output, document.samplers[index].metadata);
        output.push_back('}');
    }
    output.push_back(']');
}

void append_buffers(std::string& output, const ExportPackage& package) {
    if (package.views.empty()) {
        return;
    }
    std::uint32_t byte_length = 0;
    for (const ByteRange& view : package.views) {
        byte_length = std::max(byte_length, view.offset + view.length);
    }
    output.append(",\"buffers\":[{");
    if (!package.glb) {
        output.append("\"uri\":");
        append_string(output, package.buffer_uri);
        output.push_back(',');
    }
    output.append("\"byteLength\":");
    append_unsigned(output, byte_length);
    output.append("}],\"bufferViews\":[");
    for (std::size_t index = 0; index < package.views.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        const ByteRange& view = package.views[index];
        output.append("{\"buffer\":0");
        if (view.offset != 0U) {
            output.append(",\"byteOffset\":");
            append_unsigned(output, view.offset);
        }
        output.append(",\"byteLength\":");
        append_unsigned(output, view.length);
        output.push_back('}');
    }
    output.push_back(']');
}

void append_accessors(std::string& output, const std::vector<Accessor>& accessors) {
    if (accessors.empty()) {
        return;
    }
    output.append(",\"accessors\":[");
    for (std::size_t index = 0; index < accessors.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        const Accessor& accessor = accessors[index];
        output.append("{\"bufferView\":");
        append_unsigned(output, accessor.view);
        output.append(",\"componentType\":");
        append_unsigned(output, accessor.component_type);
        output.append(",\"count\":");
        append_unsigned(output, accessor.count);
        output.append(",\"type\":");
        append_string(output, accessor.type);
        if (accessor.bounds) {
            output.append(",\"min\":");
            append_float3(output, accessor.bounds->minimum);
            output.append(",\"max\":");
            append_float3(output, accessor.bounds->maximum);
        }
        output.push_back('}');
    }
    output.push_back(']');
}

[[nodiscard]] Result<void> append_scene_sections(std::string& output,
                                                 const ExportPackage& package) {
    if (const Result<void> scenes = append_scenes(output, package.document); !scenes) {
        return scenes.error();
    }
    if (const Result<void> nodes = append_nodes(output, package.document); !nodes) {
        return nodes.error();
    }
    append_cameras(output, package.document);
    return append_meshes(output, package);
}

[[nodiscard]] Result<void> append_asset_sections(std::string& output,
                                                 const ExportPackage& package) {
    if (const Result<void> materials = append_materials(output, package.document); !materials) {
        return materials.error();
    }
    if (const Result<void> textures = append_textures(output, package.document); !textures) {
        return textures.error();
    }
    return append_images(output, package);
}

} // namespace

Result<std::string> build_json(const ExportPackage& package) {
    std::string output{"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Elf3D\""};
    append_preserved_metadata(output, package.document.asset_metadata);
    output.push_back('}');
    append_extensions_used(output, package.document);
    if (const Result<void> scenes = append_scene_sections(output, package); !scenes) {
        return scenes.error();
    }
    if (const Result<void> assets = append_asset_sections(output, package); !assets) {
        return assets.error();
    }
    append_samplers(output, package.document);
    append_buffers(output, package);
    append_accessors(output, package.accessors);
    append_preserved_metadata(output, package.document.root_metadata);
    output.push_back('}');
    return format_json(output);
}

} // namespace elf3d::gltf::exporter_detail
