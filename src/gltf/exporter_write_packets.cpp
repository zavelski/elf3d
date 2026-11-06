#include "exporter_write_sdk.h"

#include "exporter_metadata_cursor.h"
#include <elf3d/core/assert.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <utility>

namespace elf3d::model::detail {
using namespace gltf::exporter_detail;
namespace {
[[nodiscard]] Result<WriteSdkPacketHeader> read_frame(const DocumentWriteSource& source,
                                                      DocumentView document,
                                                      ExportFileRequest current,
                                                      std::span<std::byte> payload)
{
    WriteSdkPacketHeader header;
    if (current.input == ExportFileInput::record) {
        const auto record = source.record(document, current.metadata.address);
        if (!record)
            return record.error();
        std::memcpy(payload.data(), &record.value(), sizeof(WriteSourceRecord));
        header.chunk = {sizeof(WriteSourceRecord), 1, 1};
    } else if (current.input == ExportFileInput::text) {
        std::array<char, write_source_chunk_bytes> text;
        const auto count = std::min(text.size(), payload.size());
        const auto chunk =
            source.read_text(document, current.metadata.text, std::span{text}.first(count));
        if (!chunk)
            return chunk.error();
        header.chunk = chunk.value();
        std::memcpy(payload.data(), text.data(), header.chunk.bytes);
    } else {
        const auto data =
            current.input == ExportFileInput::links ? current.metadata.data : current.data;
        const auto chunk = source.read_data(
            document, data, payload.first(std::min(payload.size(), write_source_chunk_bytes)));
        if (!chunk)
            return chunk.error();
        header.chunk = chunk.value();
    }
    return header;
}
[[nodiscard]] Result<std::optional<ExportMetadataCursor>>
metadata_cursor(const DocumentWriteSource& reader, DocumentView document, ExportFileRequest request)
{
    if (request.input == ExportFileInput::payload)
        return std::optional<ExportMetadataCursor>{};
    const auto record = reader.record(document, request.metadata.address);
    if (!record)
        return record.error();
    auto cursor = ExportMetadataCursor::at(reader.description(), request.metadata, record.value());
    if (!cursor)
        return cursor.error();
    return std::optional<ExportMetadataCursor>{std::move(cursor).value()};
}
[[nodiscard]] Result<void> consume_metadata(ExportMetadataCursor& cursor, ExportFileRequest current,
                                            WriteSourceChunk chunk,
                                            std::span<const std::byte> payload)
{
    if (current.input == ExportFileInput::record) {
        WriteSourceRecord record;
        std::memcpy(&record, payload.data(), sizeof(record));
        return cursor.accept_record(record);
    }
    return current.input == ExportFileInput::text ? cursor.accept_text(chunk)
                                                  : cursor.accept_links(chunk);
}
[[nodiscard]] ExportFileRequest next_metadata_request(const ExportMetadataCursor& cursor)
{
    ExportFileRequest result;
    result.metadata = cursor.request();
    if (result.metadata.input == ExportMetadataInput::text)
        result.input = ExportFileInput::text;
    else if (result.metadata.input == ExportMetadataInput::links)
        result.input = ExportFileInput::links;
    return result;
}
[[nodiscard]] Result<bool> next_request(ExportFileRequest& request, WriteSdkPacketHeader header,
                                        std::span<const std::byte> payload,
                                        std::optional<ExportMetadataCursor>& cursor)
{
    if (cursor) {
        if (const auto consumed = consume_metadata(*cursor, request, header.chunk, payload);
            !consumed)
            return consumed.error();
        if (cursor->ready())
            return false;
        request = next_metadata_request(*cursor);
        return true;
    }
    request.data.offset += header.chunk.elements;
    return request.data.offset < header.chunk.total;
}
[[nodiscard]] Result<void> provide_frame(ExportFileWriter& writer, WriteSdkPacketHeader header,
                                         std::span<const std::byte> payload)
{
    const auto request = writer.request();
    if (!request)
        return Error{ErrorCode::invalid_argument, "File writer is not expecting a source frame"};
    if (request->input == ExportFileInput::record) {
        if (payload.size() != sizeof(WriteSourceRecord))
            return Error{ErrorCode::invalid_argument, "Write record frame size is invalid"};
        WriteSourceRecord record;
        std::memcpy(&record, payload.data(), sizeof(record));
        return writer.accept_record(record);
    }
    if (request->input == ExportFileInput::text) {
        std::array<char, write_source_chunk_bytes> text;
        std::memcpy(text.data(), payload.data(), payload.size());
        return writer.accept_text(header.chunk, std::span{text}.first(payload.size()));
    }
    return writer.accept_data(header.chunk, payload);
}
[[nodiscard]] Result<WriteSdkPacketHeader> decode_frame(std::span<const std::byte> packet,
                                                        ExportFileWriter& writer,
                                                        const ModelWriteSource& source)
{
    const auto request = writer.request();
    if (!request || packet.size() < sizeof(WriteSdkPacketHeader))
        return Error{ErrorCode::invalid_argument, "File writer frame is unexpected or truncated"};
    WriteSdkPacketHeader header;
    std::memcpy(&header, packet.data(), sizeof(header));
    if (header.request != WriteSdkAccess::encode(*request, source) ||
        header.chunk.bytes > packet.size() - sizeof(header) ||
        header.chunk.bytes > write_source_chunk_bytes)
        return Error{ErrorCode::invalid_argument,
                     "File writer frame does not match its current request"};
    return header;
}
} // namespace
Result<std::size_t> WriteSdkAccess::read(const ModelWriteSource& source, DocumentView document,
                                         ModelWriteInputRequest request,
                                         std::span<std::byte> destination)
{
    auto decoded = decode(request, source);
    if (!decoded)
        return decoded.error();
    if (destination.size() < model_write_packet_capacity)
        return Error{ErrorCode::invalid_argument,
                     "Write packet storage is smaller than model_write_packet_capacity"};
    const auto input = reader(source);
    if (!input.matches(document))
        return Error{ErrorCode::invalid_argument,
                     "Write source changed or belongs to another Document"};
    auto cursor = metadata_cursor(input, document, decoded.value());
    if (!cursor)
        return cursor.error();
    destination = destination.first(std::min(destination.size(), model_write_packet_limit));
    std::size_t used = 0;
    for (unsigned frames = 0; frames < 64; ++frames) {
        auto remaining = destination.subspan(used);
        if (remaining.size() < sizeof(WriteSdkPacketHeader) + sizeof(WriteSourceRecord))
            break;
        auto payload = remaining.subspan(sizeof(WriteSdkPacketHeader));
        auto frame = read_frame(input, document, decoded.value(), payload);
        if (!frame)
            return frame.error();
        frame.value().request = encode(decoded.value(), source);
        std::memcpy(remaining.data(), &frame.value(), sizeof(WriteSdkPacketHeader));
        used += sizeof(WriteSdkPacketHeader) + frame.value().chunk.bytes;
        const auto next = next_request(decoded.value(), frame.value(),
                                       payload.first(frame.value().chunk.bytes), cursor.value());
        if (!next)
            return next.error();
        if (!next.value())
            break;
    }
    ELF3D_ASSERT(used != 0);
    return used;
}
Result<void> WriteSdkAccess::provide(ExportFileWriter& writer, const ModelWriteSource& source,
                                     std::span<const std::byte> packet)
{
    if (packet.empty() || packet.size() > model_write_packet_limit)
        return Error{ErrorCode::invalid_argument, "File writer packet size is invalid"};
    bool consumed = false;
    while (!packet.empty()) {
        const auto header = decode_frame(packet, writer, source);
        if (!header) {
            if (consumed)
                writer.reject_input(header.error());
            return header.error();
        }
        const auto size = sizeof(WriteSdkPacketHeader) + header.value().chunk.bytes;
        const auto provided =
            provide_frame(writer, header.value(),
                          packet.subspan(sizeof(WriteSdkPacketHeader), header.value().chunk.bytes));
        if (!provided) {
            writer.reject_input(provided.error());
            return provided.error();
        }
        consumed = true;
        packet = packet.subspan(size);
    }
    return {};
}
} // namespace elf3d::model::detail
