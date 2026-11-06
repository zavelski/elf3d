#include "exporter_internal.hpp"
#include "exporter_source_metadata.h"

#include <elf3d/core/assert.h>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] ModelJsonMetadataView view(const ModelJsonMetadata& value)
{
    return {value.extras_json ? std::optional<std::string_view>{*value.extras_json} : std::nullopt,
            value.extensions};
}
} // namespace

Result<WriteSourceRecord> ExportSourceMetadata::primitive_source(std::size_t index) const noexcept
{
    if (!ready() || index >= primitives_.size()) {
        return Error{ErrorCode::invalid_primitive_id, "Writer primitive source is unavailable"};
    }
    const auto& primitive = primitives_[index];
    WriteSourceRecord result;
    result.address = {WriteSourceObject::primitive, index};
    result.primitive = primitive.id;
    result.mesh = primitive.mesh;
    result.material = primitive.material;
    result.bounds = primitive.bounds;
    result.elements = primitive.elements;
    result.has_extras = primitive.metadata.extras_json.has_value();
    result.extras_bytes = result.has_extras ? primitive.metadata.extras_json->size() : 0;
    result.extensions = primitive.metadata.extensions.size();
    return result;
}

Result<WriteSourceRecord> ExportSourceMetadata::image_source(std::size_t index) const noexcept
{
    if (!ready() || index >= images_.size()) {
        return Error{ErrorCode::invalid_image_id, "Writer image source is unavailable"};
    }
    const auto& image = images_[index];
    WriteSourceRecord result;
    result.address = {WriteSourceObject::image, index};
    result.image = image.id;
    result.width = image.width;
    result.height = image.height;
    result.format = image.format;
    result.mime = image.mime;
    result.elements[0] = image.bytes[0];
    result.elements[1] = image.bytes[1];
    return result;
}

ModelJsonMetadata& ExportSourceMetadata::metadata()
{
    switch (address().object) {
    case WriteSourceObject::root:
        return root_;
    case WriteSourceObject::asset:
        return asset_;
    case WriteSourceObject::scene:
        return scenes_[cursor_.index_].metadata;
    case WriteSourceObject::node:
        return nodes_[cursor_.index_].metadata;
    case WriteSourceObject::camera:
        return nodes_[cursor_.index_].camera_metadata;
    case WriteSourceObject::camera_projection:
        return nodes_[cursor_.index_].projection_metadata;
    default:
        return resource_metadata();
    }
}

ModelJsonMetadata& ExportSourceMetadata::resource_metadata()
{
    switch (address().object) {
    case WriteSourceObject::mesh:
        return meshes_[cursor_.index_].metadata;
    case WriteSourceObject::primitive:
        return primitives_[cursor_.index_].metadata;
    case WriteSourceObject::material:
        return materials_[cursor_.index_].metadata;
    case WriteSourceObject::image:
        return images_[cursor_.index_].metadata;
    case WriteSourceObject::texture:
        return textures_[cursor_.index_].metadata;
    case WriteSourceObject::sampler:
        return samplers_[cursor_.index_].metadata;
    default:
        ELF3D_ASSERT(false);
        return root_;
    }
}

std::string& ExportSourceMetadata::name()
{
    switch (address().object) {
    case WriteSourceObject::scene:
        return scenes_[cursor_.index_].name;
    case WriteSourceObject::node:
        return nodes_[cursor_.index_].name;
    case WriteSourceObject::camera:
        return nodes_[cursor_.index_].camera_properties.name;
    case WriteSourceObject::mesh:
        return meshes_[cursor_.index_].name;
    case WriteSourceObject::material:
        return materials_[cursor_.index_].name;
    default:
        ELF3D_ASSERT(false);
        return scenes_[0].name;
    }
}

void ExportSourceMetadata::store_record(const WriteSourceRecord& record)
{
    if (address().object <= WriteSourceObject::camera_projection)
        store_structural_record(record);
    else
        store_resource_record(record);
}

void ExportSourceMetadata::store_structural_record(const WriteSourceRecord& record)
{
    switch (address().object) {
    case WriteSourceObject::scene:
        scenes_[cursor_.index_].id = record.scene;
        break;
    case WriteSourceObject::node: {
        auto& value = nodes_[cursor_.index_];
        value.id = record.node;
        value.parent = record.parent;
        value.matrix = record.local_matrix;
        value.mesh = record.node_mesh;
        value.camera = record.camera;
        break;
    }
    case WriteSourceObject::camera: {
        auto& value = nodes_[cursor_.index_].camera_properties;
        value.aspect_ratio = record.aspect_ratio;
        value.infinite_far_plane = record.infinite_far_plane;
        value.image_based_lighting = record.lighting;
        break;
    }
    default:
        break;
    }
}

void ExportSourceMetadata::store_resource_record(const WriteSourceRecord& record)
{
    switch (address().object) {
    case WriteSourceObject::mesh:
        meshes_[cursor_.index_].id = record.mesh;
        break;
    case WriteSourceObject::primitive:
        primitives_[cursor_.index_] = {record.primitive, record.mesh,     record.material,
                                       record.bounds,    record.elements, {}};
        break;
    case WriteSourceObject::material:
        materials_[cursor_.index_].id = record.material;
        materials_[cursor_.index_].description = record.material_description;
        break;
    case WriteSourceObject::image:
        images_[cursor_.index_] = {record.image,
                                   record.width,
                                   record.height,
                                   record.format,
                                   record.mime,
                                   {record.elements[0], record.elements[1]},
                                   {}};
        break;
    case WriteSourceObject::texture:
        textures_[cursor_.index_] = {record.texture, record.texture_description, {}};
        break;
    case WriteSourceObject::sampler:
        samplers_[cursor_.index_] = {record.sampler, record.sampler_description, {}};
        break;
    default:
        break;
    }
}

Result<void> ExportSourceMetadata::bind(ExportData& destination) const
{
    if (!ready())
        return Error{ErrorCode::invalid_argument, "Write metadata is incomplete"};
    destination.scenes.resize(scenes_.size());
    destination.nodes.resize(nodes_.size());
    destination.meshes.resize(meshes_.size());
    destination.primitives.resize(primitives_.size());
    destination.materials.resize(materials_.size());
    destination.images.resize(images_.size());
    destination.textures.resize(textures_.size());
    destination.samplers.resize(samplers_.size());
    destination.node_indices.clear();
    destination.mesh_indices.clear();
    destination.primitive_indices.clear();
    destination.material_indices.clear();
    destination.image_indices.clear();
    destination.texture_indices.clear();
    destination.sampler_indices.clear();
    destination.root_metadata = view(root_);
    destination.asset_metadata = view(asset_);
    destination.required_extensions = required_extensions_;
    destination.default_scene = cursor_.description_.default_scene;
    if (const auto bound = bind_structural(destination); !bound)
        return bound.error();
    bind_resources(destination);
    return {};
}

Result<void> ExportSourceMetadata::bind_structural(ExportData& destination) const
{
    for (std::size_t i = 0; i < scenes_.size(); ++i) {
        const auto& value = scenes_[i];
        destination.scenes[i] = {value.id, value.name, value.roots, view(value.metadata)};
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        const auto& value = nodes_[i];
        destination.node_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        const auto& camera = value.camera_properties;
        destination.nodes[i] = {value.id,
                                value.name,
                                value.parent,
                                value.children,
                                value.matrix,
                                value.mesh,
                                value.camera,
                                view(value.metadata),
                                {camera.name, camera.aspect_ratio, camera.infinite_far_plane,
                                 camera.image_based_lighting, view(value.camera_metadata),
                                 view(value.projection_metadata)}};
    }
    for (std::size_t i = 0; i < meshes_.size(); ++i) {
        const auto& value = meshes_[i];
        if (value.primitives.empty())
            return Error{ErrorCode::invalid_mesh_data,
                         "glTF export requires every document mesh to contain a primitive"};
        destination.mesh_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        auto& target = destination.meshes[i];
        target.id = value.id;
        target.name = value.name;
        target.primitives = value.primitives;
        target.bounds.reset();
        target.metadata = view(value.metadata);
    }
    for (std::size_t i = 0; i < primitives_.size(); ++i) {
        const auto& value = primitives_[i];
        destination.primitive_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        auto& target = destination.primitives[i];
        target.id = value.id;
        target.mesh = value.mesh;
        target.material = value.material;
        target.bounds = value.bounds;
        target.data = {};
        target.metadata = view(value.metadata);
    }
    return {};
}

void ExportSourceMetadata::bind_resources(ExportData& destination) const
{
    for (std::size_t i = 0; i < materials_.size(); ++i) {
        const auto& value = materials_[i];
        destination.material_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        destination.materials[i] = {value.id, value.description, view(value.metadata)};
        destination.materials[i].description.name = value.name;
    }
    for (std::size_t i = 0; i < images_.size(); ++i) {
        const auto& value = images_[i];
        destination.image_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        auto& target = destination.images[i];
        target.id = value.id;
        target.width = value.width;
        target.height = value.height;
        target.format = value.format;
        target.source_mime_type = value.mime;
        target.pixels = {};
        target.source_bytes = {};
        target.metadata = view(value.metadata);
    }
    for (std::size_t i = 0; i < textures_.size(); ++i) {
        const auto& value = textures_[i];
        destination.texture_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        destination.textures[i] = {value.id, value.description, view(value.metadata)};
    }
    for (std::size_t i = 0; i < samplers_.size(); ++i) {
        const auto& value = samplers_[i];
        destination.sampler_indices.push_back({value.id, static_cast<std::uint32_t>(i)});
        destination.samplers[i] = {value.id, value.description, view(value.metadata)};
    }
}
} // namespace elf3d::gltf::exporter_detail
