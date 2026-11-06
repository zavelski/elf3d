#include "exporter_metadata_cursor.h"

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] bool has_name(WriteSourceObject object)
{
    switch (object) {
    case WriteSourceObject::scene:
    case WriteSourceObject::node:
    case WriteSourceObject::camera:
    case WriteSourceObject::mesh:
    case WriteSourceObject::material:
        return true;
    default:
        return false;
    }
}
[[nodiscard]] bool valid_record_shape(const WriteSourceRecord& record)
{
    if (record.material_description.name.data() != nullptr ||
        !record.material_description.name.empty())
        return false;
    if (record.name_bytes != 0 && !has_name(record.address.object))
        return false;
    return record.links == 0 || record.address.object == WriteSourceObject::scene ||
           record.address.object == WriteSourceObject::node ||
           record.address.object == WriteSourceObject::mesh;
}
} // namespace

WriteSourceAddress ExportMetadataCursor::address() const noexcept
{
    return {static_cast<WriteSourceObject>(category_), index_};
}
bool ExportMetadataCursor::ready() const noexcept
{
    return phase_ == Phase::complete;
}

ExportMetadataRequest ExportMetadataCursor::request() const noexcept
{
    ExportMetadataRequest result;
    result.address = address();
    if (ready()) {
        result.input = ExportMetadataInput::complete;
        return result;
    }
    if (phase_ == Phase::record)
        return result;
    if (phase_ == Phase::links) {
        result.input = ExportMetadataInput::links;
        auto kind = WriteSourceData::children;
        if (category_ == 2)
            kind = WriteSourceData::roots;
        if (category_ == 6)
            kind = WriteSourceData::primitives;
        result.data = {address(), kind, offset_};
        return result;
    }
    result.input = ExportMetadataInput::text;
    auto kind = WriteSourceText::name;
    switch (phase_) {
    case Phase::extras:
        kind = WriteSourceText::extras;
        break;
    case Phase::extension_name:
        kind = WriteSourceText::extension_name;
        break;
    case Phase::extension_data:
        kind = WriteSourceText::extension_data;
        break;
    case Phase::required_extension:
        kind = WriteSourceText::required_extension;
        break;
    default:
        break;
    }
    result.text = {address(), kind, item_, offset_};
    return result;
}

void ExportMetadataCursor::next_field(Phase after)
{
    offset_ = 0;
    text_size_.reset();
    if (after == Phase::name && current_.name_bytes > 0) {
        phase_ = Phase::name;
        return;
    }
    if (after <= Phase::extras && current_.has_extras) {
        phase_ = Phase::extras;
        return;
    }
    if (after <= Phase::extension_name && current_.extensions > 0) {
        phase_ = Phase::extension_name;
        item_ = 0;
        return;
    }
    if (current_.links > 0) {
        phase_ = Phase::links;
        return;
    }
    next_record();
}

void ExportMetadataCursor::next_record()
{
    ++index_;
    offset_ = 0;
    item_ = 0;
    text_size_.reset();
    while (category_ < description_.counts.size() && index_ >= description_.counts[category_]) {
        ++category_;
        index_ = 0;
    }
    if (category_ == description_.counts.size()) {
        if (description_.required_extensions > 0) {
            category_ = 0;
            index_ = 0;
            phase_ = Phase::required_extension;
            return;
        }
        phase_ = Phase::complete;
        return;
    }
    phase_ = Phase::record;
}

namespace {
[[nodiscard]] bool valid_text_remainder(WriteSourceChunk chunk, std::size_t offset)
{
    return offset <= chunk.total && chunk.bytes <= chunk.total - offset &&
           (chunk.bytes != 0 || offset == chunk.total);
}
[[nodiscard]] bool valid_link_packet(WriteSourceChunk chunk, std::size_t total, std::size_t offset,
                                     std::size_t bytes)
{
    return chunk.bytes == bytes && chunk.bytes <= write_source_chunk_bytes &&
           chunk.total == total && offset <= total && chunk.elements <= total - offset &&
           chunk.elements != 0;
}
} // namespace

Result<void> ExportMetadataCursor::validate_text_sequence(WriteSourceChunk chunk)
{
    if (!text_size_) {
        if (phase_ == Phase::name && chunk.total != current_.name_bytes)
            return Error{ErrorCode::invalid_argument, "Write metadata name size changed"};
        if (phase_ == Phase::extras && chunk.total != current_.extras_bytes)
            return Error{ErrorCode::invalid_argument, "Write metadata extras size changed"};
        text_size_ = chunk.total;
    }
    if (chunk.total != *text_size_ || !valid_text_remainder(chunk, offset_))
        return Error{ErrorCode::invalid_argument, "Write metadata text sequence is inconsistent"};
    return {};
}

void ExportMetadataCursor::finish_text()
{
    offset_ = 0;
    text_size_.reset();
    switch (phase_) {
    case Phase::name:
        next_field(Phase::extras);
        break;
    case Phase::extras:
        next_field(Phase::extension_name);
        break;
    case Phase::extension_name:
        phase_ = Phase::extension_data;
        break;
    case Phase::extension_data:
        ++item_;
        if (item_ < current_.extensions)
            phase_ = Phase::extension_name;
        else
            next_field(Phase::links);
        break;
    case Phase::required_extension:
        ++item_;
        if (item_ == description_.required_extensions)
            phase_ = Phase::complete;
        break;
    default:
        break;
    }
}

Result<ExportMetadataCursor> ExportMetadataCursor::at(WriteSourceDescription source,
                                                      ExportMetadataRequest request,
                                                      WriteSourceRecord record)
{
    ExportMetadataCursor result;
    result.description_ = source;
    result.current_ = record;
    result.category_ = static_cast<std::size_t>(request.address.object);
    result.index_ = request.address.index;
    if (result.category_ >= source.counts.size() ||
        result.index_ >= source.counts[result.category_])
        return Error{ErrorCode::invalid_argument, "Metadata cursor address is outside its source"};
    if (request.input == ExportMetadataInput::links) {
        result.phase_ = Phase::links;
        result.offset_ = request.data.offset;
    } else if (request.input == ExportMetadataInput::text) {
        result.item_ = request.text.item;
        result.offset_ = request.text.offset;
        switch (request.text.text) {
        case WriteSourceText::name:
            result.phase_ = Phase::name;
            break;
        case WriteSourceText::extras:
            result.phase_ = Phase::extras;
            break;
        case WriteSourceText::extension_name:
            result.phase_ = Phase::extension_name;
            break;
        case WriteSourceText::extension_data:
            result.phase_ = Phase::extension_data;
            break;
        case WriteSourceText::required_extension:
            result.phase_ = Phase::required_extension;
            break;
        }
    }
    return result;
}
Result<void> ExportMetadataCursor::validate_record(const WriteSourceRecord& record) const
{
    if (phase_ != Phase::record || record.address.object != address().object ||
        record.address.index != index_ || !valid_record_shape(record))
        return Error{ErrorCode::invalid_argument,
                     "Write metadata record is unexpected or contains a borrowed name"};
    return {};
}
Result<void> ExportMetadataCursor::accept_record(const WriteSourceRecord& record)
{
    if (const auto valid = validate_record(record); !valid)
        return valid.error();
    current_ = record;
    next_field(Phase::name);
    return {};
}
Result<void> ExportMetadataCursor::accept_text(WriteSourceChunk chunk)
{
    if (request().input != ExportMetadataInput::text || chunk.bytes != chunk.elements ||
        chunk.bytes > write_source_chunk_bytes)
        return Error{ErrorCode::invalid_argument, "Write metadata text packet is invalid"};
    if (const auto valid = validate_text_sequence(chunk); !valid)
        return valid.error();
    offset_ += chunk.bytes;
    if (offset_ == chunk.total)
        finish_text();
    return {};
}
Result<void> ExportMetadataCursor::accept_links(WriteSourceChunk chunk)
{
    if (phase_ != Phase::links || !valid_link_packet(chunk, current_.links, offset_, chunk.bytes))
        return Error{ErrorCode::invalid_argument,
                     "Write metadata relationship sequence is invalid"};
    const auto stride =
        address().object == WriteSourceObject::mesh ? sizeof(PrimitiveId) : sizeof(NodeId);
    if (chunk.elements > write_source_chunk_elements || chunk.elements > chunk.bytes / stride ||
        chunk.elements * stride != chunk.bytes)
        return Error{ErrorCode::invalid_argument, "Write metadata relationship stride is invalid"};
    offset_ += chunk.elements;
    if (offset_ == chunk.total)
        next_record();
    return {};
}
} // namespace elf3d::gltf::exporter_detail
