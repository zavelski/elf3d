#include "exporter_source_metadata.h"

#include <elf3d/core/assert.h>

#include <cstring>
#include <limits>
#include <utility>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;

Result<std::unique_ptr<ExportSourceMetadata>>
ExportSourceMetadata::create(WriteSourceDescription description)
{
    for (const auto count : description.counts) {
        if (count > std::numeric_limits<std::uint32_t>::max())
            return Error{ErrorCode::size_overflow,
                         "Write metadata table exceeds the 32-bit output limit"};
    }
    if (description.counts[0] != 1 || description.counts[1] != 1 ||
        description.counts[3] != description.counts[4] ||
        description.counts[3] != description.counts[5])
        return Error{ErrorCode::invalid_argument, "Write metadata scope counts are inconsistent"};
    auto result = std::make_unique<ExportSourceMetadata>();
    result->cursor_.description_ = description;
    result->scenes_.resize(description.counts[2]);
    result->nodes_.resize(description.counts[3]);
    result->meshes_.resize(description.counts[6]);
    result->primitives_.resize(description.counts[7]);
    result->materials_.resize(description.counts[8]);
    result->images_.resize(description.counts[9]);
    result->textures_.resize(description.counts[10]);
    result->samplers_.resize(description.counts[11]);
    result->required_extensions_.resize(description.required_extensions);
    return result;
}

WriteSourceAddress ExportSourceMetadata::address() const noexcept
{
    return cursor_.address();
}
bool ExportSourceMetadata::ready() const noexcept
{
    return cursor_.ready();
}
ExportMetadataRequest ExportSourceMetadata::request() const noexcept
{
    return cursor_.request();
}
Result<void> ExportSourceMetadata::accept_record(const WriteSourceRecord& record)
{
    if (const auto valid = cursor_.validate_record(record); !valid)
        return valid.error();
    store_record(record);
    metadata().extensions.resize(record.extensions);
    if (record.has_extras)
        metadata().extras_json.emplace();
    return cursor_.accept_record(record);
}

std::string& ExportSourceMetadata::text()
{
    switch (cursor_.phase_) {
    case Phase::name:
        return name();
    case Phase::extras:
        return *metadata().extras_json;
    case Phase::extension_name:
        return metadata().extensions[cursor_.item_].name;
    case Phase::extension_data:
        return metadata().extensions[cursor_.item_].data;
    case Phase::required_extension:
        return required_extensions_[cursor_.item_];
    default:
        ELF3D_ASSERT(false);
        return required_extensions_[0];
    }
}

Result<void> ExportSourceMetadata::accept_text(WriteSourceChunk chunk, std::span<const char> bytes)
{
    if (request().input != ExportMetadataInput::text || chunk.bytes != bytes.size())
        return Error{ErrorCode::invalid_argument, "Write metadata text packet is invalid"};
    auto& destination = text();
    if (const auto valid = cursor_.accept_text(chunk); !valid)
        return valid.error();
    if (!bytes.empty())
        destination.append(bytes.data(), bytes.size());
    return {};
}

template <typename T>
[[nodiscard]] Result<void> append_links(std::vector<T>& destination, WriteSourceChunk chunk,
                                        std::span<const std::byte> bytes)
{
    if (chunk.elements > write_source_chunk_elements ||
        chunk.elements > write_source_chunk_bytes / sizeof(T) ||
        chunk.elements * sizeof(T) != bytes.size())
        return Error{ErrorCode::invalid_argument,
                     "Write metadata relationship packet has an invalid size"};
    const auto offset = destination.size();
    destination.resize(offset + chunk.elements);
    if (!bytes.empty())
        std::memcpy(destination.data() + offset, bytes.data(), bytes.size());
    return {};
}

Result<void> ExportSourceMetadata::accept_links(WriteSourceChunk chunk,
                                                std::span<const std::byte> bytes)
{
    if (chunk.bytes != bytes.size())
        return Error{ErrorCode::invalid_argument,
                     "Write metadata relationship packet size changed"};
    const auto object = address().object;
    const auto index = cursor_.index_;
    if (const auto valid = cursor_.accept_links(chunk); !valid)
        return valid.error();
    switch (object) {
    case WriteSourceObject::scene:
        return append_links(scenes_[index].roots, chunk, bytes);
    case WriteSourceObject::node:
        return append_links(nodes_[index].children, chunk, bytes);
    case WriteSourceObject::mesh:
        return append_links(meshes_[index].primitives, chunk, bytes);
    default:
        return Error{ErrorCode::invalid_argument, "Write object has no relationship array"};
    }
}

Result<std::unique_ptr<ExportSourceMetadata>> ExportSourceMetadata::capture(DocumentView document)
{
    const auto source = DocumentWriteSource::create(document);
    if (!source)
        return source.error();
    auto result = create(source.value().description());
    if (!result)
        return result.error();
    std::array<std::byte, write_source_chunk_bytes> bytes;
    while (!result.value()->ready()) {
        const auto request = result.value()->request();
        Result<void> accepted;
        if (request.input == ExportMetadataInput::record) {
            const auto record = source.value().record(document, request.address);
            if (!record)
                return record.error();
            accepted = result.value()->accept_record(record.value());
        } else if (request.input == ExportMetadataInput::text) {
            std::array<char, write_source_chunk_bytes> text;
            const auto chunk = source.value().read_text(document, request.text, text);
            if (!chunk)
                return chunk.error();
            accepted = result.value()->accept_text(chunk.value(),
                                                   std::span{text}.first(chunk.value().bytes));
        } else {
            const auto chunk = source.value().read_data(document, request.data, bytes);
            if (!chunk)
                return chunk.error();
            accepted = result.value()->accept_links(chunk.value(),
                                                    std::span{bytes}.first(chunk.value().bytes));
        }
        if (!accepted)
            return accepted.error();
    }
    return std::move(result);
}
} // namespace elf3d::gltf::exporter_detail
