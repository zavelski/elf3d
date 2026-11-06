#include "exporter_image_input.h"
#include "exporter_png_encoder.h"

#include <elf3d/core/assert.h>
#include <image/image.h>
#include <model/document_storage.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] Result<void> validate_encoded_only(const WriteSourceRecord& source)
{
    if (source.width != 0 || source.height != 0 || source.elements[0] != 0 ||
        source.elements[1] < 8 || source.elements[1] > image::maximum_encoded_bytes)
        return Error{ErrorCode::invalid_argument, "Encoded-only image description is invalid"};
    return {};
}
[[nodiscard]] Result<void> validate_source_stream(const WriteSourceRecord& source)
{
    if (source.mime == ModelImageMimeType::none) {
        if (source.elements[1] != 0)
            return Error{ErrorCode::invalid_argument,
                         "Image source MIME is absent for nonempty data"};
        return {};
    }
    const bool supported = source.mime == ModelImageMimeType::png ||
                           source.mime == ModelImageMimeType::jpeg ||
                           source.mime == ModelImageMimeType::tiff;
    if (!supported || source.elements[1] == 0 || source.elements[1] > image::maximum_encoded_bytes)
        return Error{ErrorCode::invalid_argument, "Image source stream description is invalid"};
    return {};
}
[[nodiscard]] Result<void> validate_shape(const WriteSourceRecord& source)
{
    if (source.mime == ModelImageMimeType::exr)
        return validate_encoded_only(source);
    const auto expected = expected_image_bytes(source.width, source.height, source.format);
    if (!expected)
        return expected.error();
    if (expected.value() != source.elements[0])
        return Error{ErrorCode::invalid_argument,
                     "Image pixels do not match tightly packed RGBA8 rows"};
    return validate_source_stream(source);
}
[[nodiscard]] Result<void> validate_source(const WriteSourceRecord& source,
                                           const ExportData& metadata)
{
    if (source.address.object != WriteSourceObject::image ||
        source.address.index >= metadata.images.size())
        return Error{ErrorCode::invalid_image_id,
                     "Image input address is outside the captured table"};
    if (source.material_description.name.data() != nullptr ||
        !source.material_description.name.empty())
        return Error{ErrorCode::invalid_argument,
                     "Image input description contains a borrowed name"};
    const auto& image = metadata.images[source.address.index];
    if (image.id != source.image || image.width != source.width || image.height != source.height ||
        image.format != source.format || image.source_mime_type != source.mime)
        return Error{ErrorCode::invalid_image_id, "Image input does not match captured metadata"};
    return validate_shape(source);
}
} // namespace
struct ExportImageInput::Impl {
    enum class Phase {
        capture_source,
        decode,
        compare_pixels,
        preserve_source,
        png_pixels,
        complete,
        taken,
        disposed
    };
    WriteSourceRecord source;
    Phase phase = Phase::capture_source;
    std::size_t offset = 0;
    std::vector<std::byte> encoded;
    std::optional<image::DecodedImage> decoded;
    std::optional<ExportPngEncoder> encoder;
    std::array<std::byte, 8> prefix{};
    std::size_t prefix_size = 0, peak_codec = 0;
    bool pixels_match = true;
    EncodedImageOutput output;

    [[nodiscard]] ExportImageMemory memory() const noexcept
    {
        return {encoded.capacity(), decoded ? decoded->pixels.capacity() : 0, peak_codec};
    }
    void remember_peak() noexcept
    {
        const auto current = memory();
        peak_codec = std::max(peak_codec, current.encoded_candidate + current.decoded_candidate);
    }
    void release_candidates() noexcept
    {
        std::vector<std::byte>{}.swap(encoded);
        decoded.reset();
    }
    [[nodiscard]] Result<void> begin_png()
    {
        release_candidates();
        auto started = ExportPngEncoder::create(source.width, source.height, output.data);
        if (!started)
            return started.error();
        encoder = std::move(started).value();
        output.mime_type = ModelImageMimeType::png;
        output.reencoded = true;
        phase = Phase::png_pixels;
        offset = 0;
        return {};
    }
    [[nodiscard]] Result<void> initialize(const OutputPath& target)
    {
        if (const auto opened = output.data.open(target); !opened)
            return opened.error();
        if (source.mime == ModelImageMimeType::none)
            return begin_png();
        output.mime_type = source.mime;
        if (source.mime == ModelImageMimeType::exr)
            phase = Phase::preserve_source;
        else {
            encoded.resize(source.elements[1]);
            remember_peak();
        }
        return {};
    }
    [[nodiscard]] std::optional<WriteSourceDataRequest> request() const noexcept
    {
        switch (phase) {
        case Phase::capture_source:
        case Phase::preserve_source:
            return WriteSourceDataRequest{source.address, WriteSourceData::image_source, offset};
        case Phase::compare_pixels:
        case Phase::png_pixels:
            return WriteSourceDataRequest{source.address, WriteSourceData::image_pixels, offset};
        default:
            return std::nullopt;
        }
    }
    [[nodiscard]] std::size_t total() const noexcept
    {
        return phase == Phase::capture_source || phase == Phase::preserve_source
                   ? source.elements[1]
                   : source.elements[0];
    }
    [[nodiscard]] bool matches(WriteSourceDataRequest value) const noexcept
    {
        const auto expected = request();
        return expected && value.address.object == expected->address.object &&
               value.address.index == expected->address.index && value.data == expected->data &&
               value.offset == expected->offset;
    }
    [[nodiscard]] bool valid_chunk(WriteSourceChunk chunk, std::size_t bytes) const noexcept
    {
        if (chunk.bytes != bytes || chunk.bytes == 0 || chunk.bytes > write_source_chunk_bytes ||
            chunk.elements != chunk.bytes)
            return false;
        return chunk.total == total() && offset <= chunk.total &&
               chunk.bytes <= chunk.total - offset;
    }
    [[nodiscard]] Result<void> decode_source()
    {
        if (!source_bytes_match(source.mime, encoded))
            return Error{ErrorCode::invalid_argument,
                         "Image source signature does not match its captured MIME"};
        auto candidate = image::decode_image(encoded);
        if (candidate) {
            decoded = std::move(candidate).value();
            remember_peak();
        }
        std::vector<std::byte>{}.swap(encoded);
        if (!decoded || decoded->width != source.width || decoded->height != source.height ||
            decoded->pixels.size() != source.elements[0])
            return begin_png();
        phase = Phase::compare_pixels;
        offset = 0;
        return {};
    }
    [[nodiscard]] Result<void> preserve(std::span<const std::byte> bytes)
    {
        const auto count = std::min(bytes.size(), prefix.size() - prefix_size);
        std::copy_n(bytes.begin(), count, prefix.begin() + prefix_size);
        prefix_size += count;
        output.data.append(bytes);
        if (const auto written = output.data.status(); !written)
            return written.error();
        offset += bytes.size();
        if (offset == total()) {
            if (!source_bytes_match(source.mime, std::span{prefix}.first(prefix_size)))
                return Error{ErrorCode::invalid_argument,
                             "Preserved image source signature is invalid"};
            phase = Phase::complete;
        }
        return {};
    }
    [[nodiscard]] Result<void> compare(std::span<const std::byte> bytes)
    {
        ELF3D_ASSERT(decoded);
        const auto current = std::span{decoded->pixels}.subspan(offset, bytes.size());
        pixels_match = pixels_match && std::equal(bytes.begin(), bytes.end(), current.begin());
        offset += bytes.size();
        if (offset == total()) {
            // A source packet can contain subsequent frames from this pass.
            // Restart for PNG only after consuming the complete pixel stream.
            if (!pixels_match)
                return begin_png();
            release_candidates();
            phase = Phase::preserve_source;
            offset = 0;
        }
        return {};
    }
    [[nodiscard]] Result<void> encode(std::span<const std::byte> bytes)
    {
        ELF3D_ASSERT(encoder);
        if (const auto accepted = encoder->accept(bytes, output.data); !accepted)
            return accepted.error();
        offset += bytes.size();
        if (offset == total()) {
            if (const auto finished = encoder->finish(output.data); !finished)
                return finished.error();
            encoder.reset();
            phase = Phase::complete;
        }
        return {};
    }
    [[nodiscard]] Result<void> consume(std::span<const std::byte> bytes)
    {
        switch (phase) {
        case Phase::capture_source:
            std::memcpy(encoded.data() + offset, bytes.data(), bytes.size());
            offset += bytes.size();
            if (offset == total())
                phase = Phase::decode;
            return {};
        case Phase::compare_pixels:
            return compare(bytes);
        case Phase::preserve_source:
            return preserve(bytes);
        default:
            return encode(bytes);
        }
    }
};
ExportImageInput::ExportImageInput() noexcept = default;
ExportImageInput::~ExportImageInput() noexcept = default;
ExportImageInput::ExportImageInput(ExportImageInput&&) noexcept = default;
ExportImageInput& ExportImageInput::operator=(ExportImageInput&&) noexcept = default;
Result<ExportImageInput> ExportImageInput::create(WriteSourceRecord source,
                                                  const ExportData& metadata,
                                                  const OutputPath& target)
{
    if (const auto valid = validate_source(source, metadata); !valid)
        return valid.error();
    ExportImageInput result;
    result.impl_ = std::make_unique<Impl>();
    result.impl_->source = source;
    if (const auto initialized = result.impl_->initialize(target); !initialized) {
        const auto discarded = result.discard();
        if (!discarded)
            return discarded.error();
        return initialized.error();
    }
    return result;
}
std::optional<WriteSourceDataRequest> ExportImageInput::request() const noexcept
{
    return impl_ ? impl_->request() : std::nullopt;
}
Result<void> ExportImageInput::accept(WriteSourceDataRequest value, WriteSourceChunk chunk,
                                      std::span<const std::byte> bytes)
{
    if (!impl_ || !impl_->matches(value) || !impl_->valid_chunk(chunk, bytes.size()))
        return Error{ErrorCode::invalid_argument,
                     "Image input packet has an invalid size, stream or sequence"};
    return impl_->consume(bytes);
}
bool ExportImageInput::needs_advance() const noexcept
{
    return impl_ && impl_->phase == Impl::Phase::decode;
}
Result<void> ExportImageInput::advance()
{
    if (!needs_advance())
        return Error{ErrorCode::invalid_argument, "Image input has no pending decode work"};
    return impl_->decode_source();
}
bool ExportImageInput::ready() const noexcept
{
    return impl_ && impl_->phase == Impl::Phase::complete;
}
ExportImageMemory ExportImageInput::memory() const noexcept
{
    return impl_ ? impl_->memory() : ExportImageMemory{};
}
Result<EncodedImageOutput> ExportImageInput::take_output()
{
    if (!ready())
        return Error{ErrorCode::invalid_argument, "Image output is not complete"};
    impl_->phase = Impl::Phase::taken;
    return std::move(impl_->output);
}
Result<void> ExportImageInput::discard() noexcept
{
    if (!impl_)
        return {};
    impl_->encoder.reset();
    impl_->release_candidates();
    impl_->phase = Impl::Phase::disposed;
    return impl_->output.data.discard();
}
} // namespace elf3d::gltf::exporter_detail
