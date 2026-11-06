#include "document_write_source_internal.h"

#include <model/document_storage.h>

namespace elf3d::model::detail {

Result<DocumentWriteSource> DocumentWriteSource::create(DocumentView source) noexcept
{
    if (!source.document_ || !source.document_->storage_) {
        return Error{ErrorCode::invalid_argument, "Write source requires a live Document"};
    }
    DocumentWriteSource result;
    result.owner_ = source.document_->storage_->token();
    result.revision_ = source.revision();
    result.description_.counts = {1,
                                  1,
                                  source.scene_count(),
                                  source.node_count(),
                                  source.node_count(),
                                  source.node_count(),
                                  source.mesh_count(),
                                  source.primitive_count(),
                                  source.material_count(),
                                  source.image_count(),
                                  source.texture_count(),
                                  source.sampler_count()};
    result.description_.default_scene = source.default_scene();
    result.description_.required_extensions = source.required_extensions().size();
    result.description_.preserved_metadata_stale = source.preserved_metadata_stale();
    return result;
}

bool DocumentWriteSource::matches(DocumentView source) const noexcept
{
    return source.document_ && source.document_->storage_ &&
           owner_ == source.document_->storage_->token() && revision_ == source.revision();
}

std::uint64_t DocumentWriteSource::revision() const noexcept
{
    return revision_;
}

DocumentOwnerToken DocumentWriteSource::owner() const noexcept
{
    return owner_;
}
DocumentWriteSource DocumentWriteSource::restore(DocumentOwnerToken owner, std::uint64_t revision,
                                                 WriteSourceDescription description) noexcept
{
    DocumentWriteSource source;
    source.owner_ = owner;
    source.revision_ = revision;
    source.description_ = description;
    return source;
}

WriteSourceDescription DocumentWriteSource::description() const noexcept
{
    return description_;
}

bool DocumentWriteSource::valid_address(WriteSourceAddress address) const noexcept
{
    const auto category = static_cast<std::size_t>(address.object);
    return category < description_.counts.size() && address.index < description_.counts[category];
}

Result<WriteSourceRecord> DocumentWriteSource::record(DocumentView source,
                                                      WriteSourceAddress address) const noexcept
{
    if (!matches(source) || !valid_address(address)) {
        return Error{ErrorCode::invalid_argument,
                     "Write record source changed or its address is invalid"};
    }
    auto record = address.object <= WriteSourceObject::camera_projection
                      ? write_structural_record(source, address)
                      : write_resource_record(source, address);
    if (!record) {
        return record.error();
    }
    auto& value = record.value();
    const auto metadata = write_source_metadata(source, address);
    if (!metadata) {
        return metadata.error();
    }
    value.has_extras = metadata.value().extras_json.has_value();
    value.extras_bytes = value.has_extras ? metadata.value().extras_json->size() : 0;
    value.extensions = metadata.value().extensions.size();
    return record;
}

Result<WriteSourceChunk>
DocumentWriteSource::read_data(DocumentView source, WriteSourceDataRequest request,
                               std::span<std::byte> destination) const noexcept
{
    if (!matches(source) || !valid_address(request.address)) {
        return Error{ErrorCode::invalid_argument,
                     "Write data source changed or its address is invalid"};
    }
    if (request.data == WriteSourceData::vertices)
        return write_index_data(source, request, destination);
    if (request.data <= WriteSourceData::colors) {
        return write_vertex_data(source, request, destination);
    }
    if (request.data <= WriteSourceData::corners) {
        return write_index_data(source, request, destination);
    }
    return write_resource_data(source, request, destination);
}

Result<WriteSourceChunk> DocumentWriteSource::read_text(DocumentView source,
                                                        WriteSourceTextRequest request,
                                                        std::span<char> destination) const noexcept
{
    if (!matches(source) || !valid_address(request.address)) {
        return Error{ErrorCode::invalid_argument,
                     "Write text source changed or its address is invalid"};
    }
    const auto text = write_source_text(source, request);
    if (!text) {
        return text.error();
    }
    return copy_write_source(std::as_bytes(std::span{text.value().data(), text.value().size()}),
                             request.offset, std::as_writable_bytes(destination));
}
} // namespace elf3d::model::detail
