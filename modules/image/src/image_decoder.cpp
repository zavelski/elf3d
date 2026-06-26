module;

#include <elf3d/core/error.h>

#include <stb_image.h>

#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

module elf.image;

namespace elf3d::image {
namespace {

struct StbiPixelsDeleter {
    void operator()(stbi_uc *pixels) const noexcept {
        stbi_image_free(pixels);
    }
};

using StbiPixels = std::unique_ptr<stbi_uc, StbiPixelsDeleter>;

[[nodiscard]] Error decode_error(std::string_view prefix) noexcept {
    const char *reason = stbi_failure_reason();
    const std::string copied_reason = reason != nullptr ? reason : "unknown decoder failure";
    return Error{ErrorCode::image_decode_failed, std::string{prefix} + ": " + copied_reason};
}

} // namespace

Result<DecodedImage> decode_png_or_jpeg(std::span<const std::byte> encoded) noexcept {
    if (encoded.empty()) {
        return Error{ErrorCode::image_decode_failed, "Image decoding requires encoded bytes"};
    }
    if (encoded.size() > maximum_encoded_bytes ||
        encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Encoded image exceeds the 64 MiB decoder limit"};
    }

    try {
        const auto *bytes = reinterpret_cast<const stbi_uc *>(encoded.data());
        const int byte_count = static_cast<int>(encoded.size());
        int width = 0;
        int height = 0;
        int source_channels = 0;
        if (stbi_info_from_memory(bytes, byte_count, &width, &height, &source_channels) == 0) {
            return decode_error("Image header decoding failed");
        }
        if (width <= 0 || height <= 0) {
            return Error{ErrorCode::zero_image_dimensions,
                         "Decoded images require positive width and height"};
        }
        if (static_cast<std::uint32_t>(width) > maximum_dimension ||
            static_cast<std::uint32_t>(height) > maximum_dimension) {
            return Error{ErrorCode::excessive_image_dimensions,
                         "Image dimensions exceed the 16384 pixel decoder limit"};
        }

        const std::size_t width_value = static_cast<std::size_t>(width);
        const std::size_t height_value = static_cast<std::size_t>(height);
        if (width_value > maximum_decoded_bytes / 4 ||
            height_value > maximum_decoded_bytes / (width_value * 4)) {
            return Error{ErrorCode::decoded_image_size_overflow,
                         "Decoded RGBA8 image size exceeds the 256 MiB limit"};
        }
        const std::size_t decoded_size = width_value * height_value * 4;

        StbiPixels pixels{stbi_load_from_memory(bytes, byte_count, &width, &height,
                                                &source_channels, STBI_rgb_alpha)};
        if (!pixels) {
            return decode_error("PNG/JPEG pixel decoding failed");
        }

        DecodedImage result;
        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(height);
        result.pixels.resize(decoded_size);
        std::memcpy(result.pixels.data(), pixels.get(), decoded_size);
        return result;
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Image decoding failed while allocating Elf3D-owned pixel storage"};
    }
}

} // namespace elf3d::image
