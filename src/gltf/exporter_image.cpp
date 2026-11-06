#include "exporter_image_input.h"

#include <array>
#include <span>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] Result<void> drive_image(ExportImageInput& input, const DocumentWriteSource& reader,
                                       DocumentView document)
{
    std::array<std::byte, write_source_chunk_bytes> bytes;
    while (!input.ready()) {
        if (const auto request = input.request()) {
            const auto chunk = reader.read_data(document, *request, bytes);
            if (!chunk)
                return chunk.error();
            if (const auto accepted = input.accept(*request, chunk.value(),
                                                   std::span{bytes}.first(chunk.value().bytes));
                !accepted)
                return accepted.error();
        } else {
            if (const auto advanced = input.advance(); !advanced)
                return advanced.error();
        }
    }
    return {};
}
} // namespace
Result<EncodedImageOutput> encoded_image(std::size_t index, const ExportData& metadata,
                                         DocumentView document, const OutputPath& target)
{
    const auto source = metadata.metadata_owner->image_source(index);
    const auto reader = DocumentWriteSource::create(document);
    if (!source || !reader)
        return !source ? source.error() : reader.error();
    auto input = ExportImageInput::create(source.value(), metadata, target);
    if (!input)
        return input.error();
    const auto driven = drive_image(input.value(), reader.value(), document);
    if (!driven) {
        const auto discarded = input.value().discard();
        if (!discarded)
            return discarded.error();
        return driven.error();
    }
    return input.value().take_output();
}
} // namespace elf3d::gltf::exporter_detail
