#include "document_write_source_internal.h"

namespace elf3d::model::detail {
namespace {
[[nodiscard]] Result<ModelJsonMetadataView> resource_metadata(DocumentView source,
                                                              WriteSourceAddress address)
{
    switch (address.object) {
    case WriteSourceObject::mesh:
        return source.mesh_at(address.index).value().metadata;
    case WriteSourceObject::primitive:
        return source.primitive_at(address.index).value().metadata;
    case WriteSourceObject::material:
        return source.material_at(address.index).value().metadata;
    case WriteSourceObject::image:
        return source.image_at(address.index).value().metadata;
    case WriteSourceObject::texture:
        return source.texture_at(address.index).value().metadata;
    case WriteSourceObject::sampler:
        return source.sampler_at(address.index).value().metadata;
    default:
        return Error{ErrorCode::invalid_argument, "Write metadata resource is invalid"};
    }
}
} // namespace

Result<ModelJsonMetadataView> write_source_metadata(DocumentView source, WriteSourceAddress address)
{
    switch (address.object) {
    case WriteSourceObject::root:
        return source.root_metadata();
    case WriteSourceObject::asset:
        return source.asset_metadata();
    case WriteSourceObject::scene:
        return source.scene_at(address.index).value().metadata;
    case WriteSourceObject::node:
        return source.node_at(address.index).value().metadata;
    case WriteSourceObject::camera:
        return source.node_at(address.index).value().camera_properties.metadata;
    case WriteSourceObject::camera_projection:
        return source.node_at(address.index).value().camera_properties.projection_metadata;
    default:
        return resource_metadata(source, address);
    }
}

Result<std::string_view> write_source_name(DocumentView source, WriteSourceAddress address)
{
    switch (address.object) {
    case WriteSourceObject::scene:
        return source.scene_at(address.index).value().name;
    case WriteSourceObject::node:
        return source.node_at(address.index).value().name;
    case WriteSourceObject::camera:
        return source.node_at(address.index).value().camera_properties.name;
    case WriteSourceObject::mesh:
        return source.mesh_at(address.index).value().name;
    case WriteSourceObject::material:
        return source.material_at(address.index).value().description.name;
    default:
        return std::string_view{};
    }
}

namespace {
[[nodiscard]] Result<std::string_view> extension_text(ModelJsonMetadataView metadata,
                                                      WriteSourceTextRequest request)
{
    if (request.item >= metadata.extensions.size()) {
        return Error{ErrorCode::invalid_argument, "Write extension address is invalid"};
    }
    const auto& extension = metadata.extensions[request.item];
    switch (request.text) {
    case WriteSourceText::extension_name:
        return std::string_view{extension.name};
    case WriteSourceText::extension_data:
        return std::string_view{extension.data};
    default:
        return Error{ErrorCode::invalid_argument, "Write text field is invalid"};
    }
}
} // namespace

Result<std::string_view> write_source_text(DocumentView source, WriteSourceTextRequest request)
{
    if (request.text == WriteSourceText::name) {
        return write_source_name(source, request.address);
    }
    if (request.text == WriteSourceText::required_extension) {
        const auto extensions = source.required_extensions();
        if (request.address.object != WriteSourceObject::root ||
            request.item >= extensions.size()) {
            return Error{ErrorCode::invalid_argument,
                         "Write required-extension address is invalid"};
        }
        return std::string_view{extensions[request.item]};
    }
    const auto metadata = write_source_metadata(source, request.address);
    if (!metadata) {
        return metadata.error();
    }
    if (request.text == WriteSourceText::extras) {
        if (!metadata.value().extras_json) {
            return Error{ErrorCode::invalid_argument, "Write metadata has no extras payload"};
        }
        return *metadata.value().extras_json;
    }
    return extension_text(metadata.value(), request);
}

} // namespace elf3d::model::detail
