#include <elf3d/model.h>

#include <elf3d/model/detail/document_storage.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace elf3d::model::detail {

inline constexpr std::size_t model_maximum_image_bytes = 256ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t model_maximum_source_image_bytes = 64ULL * 1024ULL * 1024ULL;

[[nodiscard]] bool png_source_bytes_match(std::span<const std::byte> bytes) noexcept
{
    constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() < signature.size()) {
        return false;
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (std::to_integer<std::uint8_t>(bytes[index]) != signature[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool jpeg_source_bytes_match(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 3U && std::to_integer<std::uint8_t>(bytes[0]) == 0xffU &&
           std::to_integer<std::uint8_t>(bytes[1]) == 0xd8U &&
           std::to_integer<std::uint8_t>(bytes[2]) == 0xffU;
}

[[nodiscard]] bool exr_signature_matches(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 8U && bytes[0] == std::byte{0x76} && bytes[1] == std::byte{0x2f} &&
           bytes[2] == std::byte{0x31} && bytes[3] == std::byte{0x01};
}

[[nodiscard]] bool tiff_signature_matches(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() < 8U) {
        return false;
    }
    constexpr std::array<std::byte, 4> little{std::byte{'I'}, std::byte{'I'}, std::byte{42},
                                              std::byte{0}};
    constexpr std::array<std::byte, 4> big{std::byte{'M'}, std::byte{'M'}, std::byte{0},
                                           std::byte{42}};
    return std::equal(little.begin(), little.end(), bytes.begin()) ||
           std::equal(big.begin(), big.end(), bytes.begin());
}

[[nodiscard]] bool source_bytes_match(ModelImageMimeType mime,
                                      std::span<const std::byte> bytes) noexcept
{
    if (mime == ModelImageMimeType::none) {
        return bytes.empty();
    }
    if (bytes.empty() || bytes.size() > model_maximum_source_image_bytes) {
        return false;
    }
    if (mime == ModelImageMimeType::png) {
        return png_source_bytes_match(bytes);
    }
    if (mime == ModelImageMimeType::tiff) {
        return tiff_signature_matches(bytes);
    }
    if (mime == ModelImageMimeType::exr) {
        return exr_signature_matches(bytes);
    }
    return mime == ModelImageMimeType::jpeg && jpeg_source_bytes_match(bytes);
}

[[nodiscard]] Result<std::size_t> expected_image_bytes(std::uint32_t image_width,
                                                       std::uint32_t image_height,
                                                       PixelFormat format) noexcept
{
    if (image_width == 0 || image_height == 0) {
        return Error{ErrorCode::zero_image_dimensions, "Images require nonzero dimensions"};
    }
    if (format != PixelFormat::rgba8_unorm) {
        return Error{ErrorCode::unsupported_texture_format,
                     "Document images currently support only RGBA8 UNORM pixels"};
    }

    const std::size_t width = image_width;
    const std::size_t height = image_height;

    if (width > model_maximum_image_bytes / 4 || height > model_maximum_image_bytes / (width * 4)) {
        return Error{ErrorCode::decoded_image_size_overflow,
                     "Image dimensions overflow the decoded byte limit"};
    }
    return width * height * 4U;
}

[[nodiscard]] Result<void>
validate_image_description(const ModelImageDescription& description) noexcept
{
    if (description.source_mime_type == ModelImageMimeType::exr) {
        if (description.width != 0U || description.height != 0U || !description.pixels.empty() ||
            !source_bytes_match(description.source_mime_type, description.source_bytes)) {
            return Error{
                ErrorCode::invalid_argument,
                "Encoded-only EXR images require zero dimensions, no pixels, and an EXR stream"};
        }
        return {};
    }
    const Result<std::size_t> expected_bytes =
        expected_image_bytes(description.width, description.height, description.format);
    if (!expected_bytes) {
        return expected_bytes.error();
    }
    if (description.pixels.size() != expected_bytes.value()) {
        return Error{ErrorCode::invalid_argument, "Image pixels must be tightly packed RGBA8 rows"};
    }
    if (!source_bytes_match(description.source_mime_type, description.source_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Image source bytes must be an optional matching PNG, JPEG, or TIFF stream"};
    }
    return {};
}

} // namespace elf3d::model::detail
