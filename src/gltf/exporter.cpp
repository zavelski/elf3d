#include <gltf/gltf.h>

#include "exporter_file_writer.h"
#include "file_path.hpp"
#include <elf3d/core/error.h>

#include <array>
#include <cstddef>
#include <new>
#include <span>
#include <utility>

namespace elf3d::gltf {
namespace {
using namespace exporter_detail;
using namespace model::detail;
[[noreturn]] void fatal_export_allocation_failure() noexcept
{
    fatal_error("Elf3D glTF exporter memory allocation failed");
}
[[noreturn]] void fatal_unexpected_export_boundary_exception() noexcept
{
    fatal_error("Elf3D glTF exporter encountered an unexpected exception");
}
[[nodiscard]] Result<void> feed_source(ExportFileWriter& writer, const DocumentWriteSource& source,
                                       DocumentView document, ExportFileRequest request)
{
    if (request.input == ExportFileInput::record) {
        const auto record = source.record(document, request.metadata.address);
        return record ? writer.accept_record(record.value()) : Result<void>{record.error()};
    }
    if (request.input == ExportFileInput::text) {
        std::array<char, write_source_chunk_bytes> text;
        const auto chunk = source.read_text(document, request.metadata.text, text);
        return chunk ? writer.accept_text(chunk.value(), std::span{text}.first(chunk.value().bytes))
                     : Result<void>{chunk.error()};
    }
    std::array<std::byte, write_source_chunk_bytes> bytes;
    const auto data =
        request.input == ExportFileInput::links ? request.metadata.data : request.data;
    const auto chunk = source.read_data(document, data, bytes);
    return chunk ? writer.accept_data(chunk.value(), std::span{bytes}.first(chunk.value().bytes))
                 : Result<void>{chunk.error()};
}
[[nodiscard]] Result<ModelWriteReport> completed_report(ExportFileOutcome outcome)
{
    if (outcome.cleanup_error)
        return *outcome.cleanup_error;
    if (outcome.error)
        return *outcome.error;
    if (!outcome.committed)
        return Error{ErrorCode::source_file_write_failed, "Document saving was cancelled"};
    return std::move(outcome.report);
}
[[nodiscard]] Result<ModelWriteReport> save(const OutputPath& path, DocumentView document,
                                            const ModelWriteOptions& options)
{
    const auto source = DocumentWriteSource::create(document);
    if (!source)
        return source.error();
    auto writer = ExportFileWriter::create(path, source.value().description(), options);
    if (!writer)
        return writer.error();
    while (!writer.value()->ready()) {
        if (const auto request = writer.value()->request()) {
            if (const auto fed = feed_source(*writer.value(), source.value(), document, *request);
                !fed)
                writer.value()->reject_input(fed.error());
        } else {
            if (const auto advanced = writer.value()->advance(); !advanced)
                writer.value()->reject_input(advanced.error());
        }
    }
    auto result = writer.value()->take_outcome();
    return result ? completed_report(std::move(result).value())
                  : Result<ModelWriteReport>{result.error()};
}
} // namespace

Result<ModelWriteReport> save_document(const std::filesystem::path& path, DocumentView document,
                                       const ModelWriteOptions& options) noexcept
{
    try {
        return save(path, document, options);
    } catch (const std::bad_alloc&) {
        fatal_export_allocation_failure();
    } catch (...) {
        fatal_unexpected_export_boundary_exception();
    }
}

} // namespace elf3d::gltf

namespace elf3d {

Result<ModelWriteReport> save_document(std::string_view path_utf8, DocumentView document,
                                       const ModelWriteOptions& options) noexcept
{
    Result<std::filesystem::path> path = gltf::file_path::from_utf8(path_utf8);
    if (!path) {
        return path.error();
    }
    return gltf::save_document(path.value(), document, options);
}

} // namespace elf3d
