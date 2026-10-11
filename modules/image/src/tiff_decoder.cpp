#include "tiff_decoder.h"

#include <elf3d/core/error.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace elf3d::image {
namespace {

struct Reader {
    std::span<const std::byte> bytes;
    bool little_endian;

    [[nodiscard]] bool contains(std::size_t offset, std::size_t count) const noexcept
    {
        return offset <= bytes.size() && count <= bytes.size() - offset;
    }

    [[nodiscard]] std::uint32_t integer(std::size_t offset, std::size_t count) const noexcept
    {
        std::uint32_t result = 0U;
        for (std::size_t index = 0U; index < count; ++index) {
            const auto shift = (little_endian ? index : count - index - 1U) * 8U;
            result |= std::to_integer<std::uint32_t>(bytes[offset + index]) << shift;
        }
        return result;
    }
};

struct Tag {
    std::uint32_t name;
    std::uint32_t type;
    std::uint32_t count;
    std::size_t entry;
};

struct Layout {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t bits;
    std::uint32_t rows_per_strip;
    Tag offsets;
    Tag sizes;
};

[[nodiscard]] Error invalid_tiff()
{
    return Error{ErrorCode::image_decode_failed,
                 "Invalid or unsupported TIFF: expected a single uncompressed, top-left, "
                 "interleaved unsigned RGB image with uniform 8-bit or 16-bit samples"};
}

[[nodiscard]] Result<std::vector<Tag>> read_directory(const Reader& reader)
{
    const std::size_t offset = reader.integer(4U, 4U);
    if (!reader.contains(offset, 2U)) {
        return invalid_tiff();
    }
    const std::size_t count = reader.integer(offset, 2U);
    if (count > 1024U || !reader.contains(offset + 2U, count * 12U + 4U)) {
        return invalid_tiff();
    }
    if (reader.integer(offset + 2U + count * 12U, 4U) != 0U) {
        return invalid_tiff();
    }
    std::vector<Tag> tags;
    tags.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const auto entry = offset + 2U + index * 12U;
        tags.push_back(Tag{reader.integer(entry, 2U), reader.integer(entry + 2U, 2U),
                           reader.integer(entry + 4U, 4U), entry});
    }
    return tags;
}

[[nodiscard]] Result<std::uint32_t> value(const Reader& reader, const Tag& tag,
                                          std::size_t index = 0U)
{
    if ((tag.type != 3U && tag.type != 4U) || tag.count == 0U || tag.count > 65536U ||
        index >= tag.count) {
        return invalid_tiff();
    }
    const std::size_t width = tag.type == 3U ? 2U : 4U;
    const std::size_t count = tag.count * width;
    const std::size_t offset = count <= 4U ? tag.entry + 8U : reader.integer(tag.entry + 8U, 4U);
    if (!reader.contains(offset, count)) {
        return invalid_tiff();
    }
    return reader.integer(offset + index * width, width);
}

[[nodiscard]] Result<std::optional<Tag>> find_tag(std::span<const Tag> tags, std::uint32_t name)
{
    std::optional<Tag> found;
    for (const auto& tag : tags) {
        if (tag.name != name) {
            continue;
        }
        if (found) {
            return invalid_tiff();
        }
        found = tag;
    }
    return found;
}

[[nodiscard]] Result<std::uint32_t> scalar(const Reader& reader, std::span<const Tag> tags,
                                           std::uint32_t name, std::uint32_t fallback)
{
    const auto tag = find_tag(tags, name);
    if (!tag) {
        return tag.error();
    }
    if (!tag.value()) {
        return fallback;
    }
    if (tag.value()->count != 1U) {
        return invalid_tiff();
    }
    return value(reader, *tag.value());
}

[[nodiscard]] Result<void> validate_encoding(const Reader& reader, std::span<const Tag> tags)
{
    // Compression, photometric interpretation, orientation, samples per pixel,
    // planar configuration, predictor, and sample format.
    constexpr std::array<std::array<std::uint32_t, 3>, 8> constraints{{
        {259U, 1U, 1U},
        {262U, 0U, 2U},
        {274U, 1U, 1U},
        {277U, 1U, 3U},
        {284U, 1U, 1U},
        {317U, 1U, 1U},
        {339U, 1U, 1U},
        {266U, 1U, 1U},
    }};
    for (const auto& constraint : constraints) {
        const auto actual = scalar(reader, tags, constraint[0], constraint[1]);
        if (!actual || actual.value() != constraint[2]) {
            return invalid_tiff();
        }
    }
    return {};
}

[[nodiscard]] Result<std::uint32_t> sample_bits(const Reader& reader, std::span<const Tag> tags)
{
    const auto tag = find_tag(tags, 258U);
    if (!tag || !tag.value() || tag.value()->count != 3U) {
        return invalid_tiff();
    }
    const auto bits = value(reader, *tag.value());
    if (!bits || (bits.value() != 8U && bits.value() != 16U)) {
        return invalid_tiff();
    }
    for (std::size_t index = 1U; index < 3U; ++index) {
        const auto channel_bits = value(reader, *tag.value(), index);
        if (!channel_bits || channel_bits.value() != bits.value()) {
            return invalid_tiff();
        }
    }
    return bits.value();
}

[[nodiscard]] Result<std::array<std::uint32_t, 2>> dimensions(const Reader& reader,
                                                              std::span<const Tag> tags)
{
    const auto width = scalar(reader, tags, 256U, 0U);
    const auto height = scalar(reader, tags, 257U, 0U);
    if (!width || !height) {
        return invalid_tiff();
    }
    if (width.value() == 0U || height.value() == 0U) {
        return invalid_tiff();
    }
    if (width.value() > maximum_dimension || height.value() > maximum_dimension ||
        height.value() > maximum_decoded_bytes / (static_cast<std::size_t>(width.value()) * 4U)) {
        return Error{ErrorCode::excessive_image_dimensions,
                     "TIFF dimensions exceed the bounded RGBA8 image limit"};
    }
    return std::array<std::uint32_t, 2>{width.value(), height.value()};
}

[[nodiscard]] Result<Tag> required_tag(std::span<const Tag> tags, std::uint32_t name)
{
    const auto tag = find_tag(tags, name);
    if (!tag || !tag.value()) {
        return invalid_tiff();
    }
    return *tag.value();
}

[[nodiscard]] Result<Layout> read_layout(const Reader& reader, std::span<const Tag> tags)
{
    const auto size = dimensions(reader, tags);
    const auto bits = sample_bits(reader, tags);
    const auto rows = scalar(reader, tags, 278U, 0xffffffffU);
    if (!size) {
        return size.error();
    }
    if (!bits || !rows || rows.value() == 0U) {
        return invalid_tiff();
    }
    const auto offsets = required_tag(tags, 273U);
    const auto counts = required_tag(tags, 279U);
    if (!offsets || !counts) {
        return invalid_tiff();
    }
    return Layout{size.value()[0], size.value()[1], bits.value(),
                  rows.value(),    offsets.value(), counts.value()};
}

[[nodiscard]] Result<void> validate_strips(const Reader& reader, const Layout& layout)
{
    const std::size_t strip_count = (layout.height - 1U) / layout.rows_per_strip + 1U;
    if (layout.offsets.count != strip_count || layout.sizes.count != strip_count) {
        return invalid_tiff();
    }
    const std::size_t row_bytes = static_cast<std::size_t>(layout.width) * 3U * (layout.bits / 8U);
    for (std::size_t index = 0U; index < strip_count; ++index) {
        const auto offset = value(reader, layout.offsets, index);
        const auto size = value(reader, layout.sizes, index);
        if (!offset || !size) {
            return invalid_tiff();
        }
        const std::size_t row = index * layout.rows_per_strip;
        const std::size_t rows = std::min<std::size_t>(layout.rows_per_strip, layout.height - row);
        if (size.value() < rows * row_bytes || !reader.contains(offset.value(), size.value())) {
            return invalid_tiff();
        }
    }
    return {};
}

void decode_strip(const Reader& reader, const Layout& layout, std::size_t start,
                  std::span<std::byte> output)
{
    const std::size_t sample_bytes = layout.bits / 8U;
    for (std::size_t pixel = 0U; pixel < output.size() / 4U; ++pixel) {
        for (std::size_t channel = 0U; channel < 3U; ++channel) {
            const auto sample =
                reader.integer(start + (pixel * 3U + channel) * sample_bytes, sample_bytes);
            const auto shift = layout.bits - 8U;
            output[pixel * 4U + channel] = static_cast<std::byte>(sample >> shift);
        }
        output[pixel * 4U + 3U] = std::byte{255};
    }
}

[[nodiscard]] DecodedImage decode_pixels(const Reader& reader, const Layout& layout)
{
    DecodedImage image{layout.width, layout.height, {}};
    const std::size_t row_bytes = static_cast<std::size_t>(layout.width) * 4U;
    image.pixels.resize(row_bytes * layout.height);
    for (std::size_t index = 0U; index < layout.offsets.count; ++index) {
        const std::size_t row = index * layout.rows_per_strip;
        const std::size_t rows = std::min<std::size_t>(layout.rows_per_strip, layout.height - row);
        const auto start = value(reader, layout.offsets, index).value();
        decode_strip(reader, layout, start,
                     std::span<std::byte>{image.pixels}.subspan(row * row_bytes, rows * row_bytes));
    }
    return image;
}

} // namespace

bool has_tiff_signature(std::span<const std::byte> encoded) noexcept
{
    if (encoded.size() < 8U) {
        return false;
    }
    constexpr std::array<std::byte, 4> little{std::byte{'I'}, std::byte{'I'}, std::byte{42},
                                              std::byte{0}};
    constexpr std::array<std::byte, 4> big{std::byte{'M'}, std::byte{'M'}, std::byte{0},
                                           std::byte{42}};
    return std::equal(little.begin(), little.end(), encoded.begin()) ||
           std::equal(big.begin(), big.end(), encoded.begin());
}

Result<DecodedImage> decode_tiff(std::span<const std::byte> encoded)
{
    if (!has_tiff_signature(encoded)) {
        return invalid_tiff();
    }
    const Reader reader{encoded, encoded[0] == std::byte{'I'}};
    const auto tags = read_directory(reader);
    if (!tags) {
        return tags.error();
    }
    if (const auto valid = validate_encoding(reader, tags.value()); !valid) {
        return valid.error();
    }
    const auto layout = read_layout(reader, tags.value());
    if (!layout) {
        return layout.error();
    }
    if (const auto strips = validate_strips(reader, layout.value()); !strips) {
        return strips.error();
    }
    return decode_pixels(reader, layout.value());
}

} // namespace elf3d::image
