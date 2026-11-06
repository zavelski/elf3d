#include "document_write_source_internal.h"

namespace elf3d::model::detail {
namespace {
[[nodiscard]] WriteSourceRecord node_record(DocumentView source, WriteSourceAddress address)
{
    const auto node = source.node_at(address.index).value();
    WriteSourceRecord result;
    result.address = address;
    result.node = node.id;
    if (address.object != WriteSourceObject::node) {
        result.present = node.perspective_camera.has_value();
        result.camera = node.perspective_camera;
        result.aspect_ratio = node.camera_properties.aspect_ratio;
        result.infinite_far_plane = node.camera_properties.infinite_far_plane;
        result.lighting = node.camera_properties.image_based_lighting;
        result.name_bytes =
            address.object == WriteSourceObject::camera ? node.camera_properties.name.size() : 0;
        return result;
    }
    result.name_bytes = node.name.size();
    result.parent = node.parent;
    result.node_mesh = node.mesh;
    result.links = node.children.size();
    result.local_matrix = node.local_matrix;
    result.camera = node.perspective_camera;
    return result;
}

[[nodiscard]] WriteSourceRecord primitive_record(DocumentView source, WriteSourceAddress address)
{
    const auto primitive = source.primitive_at(address.index).value();
    const auto data = primitive.data;
    WriteSourceRecord result;
    result.address = address;
    result.primitive = primitive.id;
    result.mesh = primitive.mesh;
    result.material = primitive.material;
    result.bounds = primitive.bounds;
    result.elements = {data.positions.size(), data.normals.size(),   data.tangents.size(),
                       data.texcoord0.size(), data.texcoord1.size(), data.colors.size(),
                       data.indices.size()};
    return result;
}

[[nodiscard]] WriteSourceRecord image_record(DocumentView source, WriteSourceAddress address)
{
    const auto image = source.image_at(address.index).value();
    WriteSourceRecord result;
    result.address = address;
    result.image = image.id;
    result.width = image.width;
    result.height = image.height;
    result.format = image.format;
    result.mime = image.source_mime_type;
    result.elements[0] = image.pixels.size();
    result.elements[1] = image.source_bytes.size();
    return result;
}
} // namespace

Result<WriteSourceRecord> write_structural_record(DocumentView source, WriteSourceAddress address)
{
    WriteSourceRecord result;
    result.address = address;
    switch (address.object) {
    case WriteSourceObject::root:
    case WriteSourceObject::asset:
        return result;
    case WriteSourceObject::scene: {
        const auto scene = source.scene_at(address.index).value();
        result.scene = scene.id;
        result.name_bytes = scene.name.size();
        result.links = scene.roots.size();
        return result;
    }
    case WriteSourceObject::node:
    case WriteSourceObject::camera:
    case WriteSourceObject::camera_projection:
        return node_record(source, address);
    default:
        return Error{ErrorCode::invalid_argument, "Write record is not a structural object"};
    }
}

Result<WriteSourceRecord> write_resource_record(DocumentView source, WriteSourceAddress address)
{
    WriteSourceRecord result;
    result.address = address;
    switch (address.object) {
    case WriteSourceObject::mesh: {
        const auto mesh = source.mesh_at(address.index).value();
        result.mesh = mesh.id;
        result.name_bytes = mesh.name.size();
        result.links = mesh.primitives.size();
        return result;
    }
    case WriteSourceObject::primitive:
        return primitive_record(source, address);
    case WriteSourceObject::image:
        return image_record(source, address);
    case WriteSourceObject::material: {
        const auto material = source.material_at(address.index).value();
        result.material = material.id;
        result.name_bytes = material.description.name.size();
        result.material_description = material.description;
        result.material_description.name = {};
        return result;
    }
    case WriteSourceObject::texture: {
        const auto texture = source.texture_at(address.index).value();
        result.texture = texture.id;
        result.texture_description = texture.description;
        return result;
    }
    case WriteSourceObject::sampler: {
        const auto sampler = source.sampler_at(address.index).value();
        result.sampler = sampler.id;
        result.sampler_description = sampler.description;
        return result;
    }
    default:
        return Error{ErrorCode::invalid_argument, "Write record is not a resource object"};
    }
}
} // namespace elf3d::model::detail
