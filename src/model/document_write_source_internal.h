#pragma once

#include "document_write_source.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace elf3d::model::detail {
[[nodiscard]] Result<WriteSourceRecord> write_structural_record(DocumentView source,
                                                                WriteSourceAddress address);
[[nodiscard]] Result<WriteSourceRecord> write_resource_record(DocumentView source,
                                                              WriteSourceAddress address);
[[nodiscard]] Result<ModelJsonMetadataView> write_source_metadata(DocumentView source,
                                                                  WriteSourceAddress address);
[[nodiscard]] Result<std::string_view> write_source_name(DocumentView source,
                                                         WriteSourceAddress address);
[[nodiscard]] Result<std::string_view> write_source_text(DocumentView source,
                                                         WriteSourceTextRequest request);
[[nodiscard]] Result<WriteSourceChunk> write_vertex_data(DocumentView source,
                                                         WriteSourceDataRequest request,
                                                         std::span<std::byte> destination);
[[nodiscard]] Result<WriteSourceChunk> write_index_data(DocumentView source,
                                                        WriteSourceDataRequest request,
                                                        std::span<std::byte> destination);
[[nodiscard]] Result<WriteSourceChunk> write_resource_data(DocumentView source,
                                                           WriteSourceDataRequest request,
                                                           std::span<std::byte> destination);

template <typename T>
[[nodiscard]] Result<WriteSourceChunk>
copy_write_source(std::span<const T> values, std::size_t offset, std::span<std::byte> destination)
{
    if (offset > values.size()) {
        return Error{ErrorCode::invalid_argument, "Write source offset exceeds its current stream"};
    }
    const auto element_limit =
        sizeof(T) == 1 ? write_source_chunk_bytes : write_source_chunk_elements;
    const auto count = std::min({values.size() - offset, destination.size() / sizeof(T),
                                 write_source_chunk_bytes / sizeof(T), element_limit});
    if (count == 0 && offset < values.size()) {
        return Error{ErrorCode::invalid_argument,
                     "Write source destination cannot hold an element"};
    }
    const auto bytes = std::as_bytes(values.subspan(offset, count));
    if (!bytes.empty()) {
        std::memcpy(destination.data(), bytes.data(), bytes.size());
    }
    return WriteSourceChunk{bytes.size(), count, values.size()};
}
} // namespace elf3d::model::detail
