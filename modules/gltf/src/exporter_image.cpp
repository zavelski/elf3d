module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "exporter_internal.hpp"

#include <png.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

module elf.gltf;

import elf.image;

namespace elf3d::gltf::exporter_detail {
namespace {

class PngImage final {
  public:
    PngImage() noexcept {
        value_.version = PNG_IMAGE_VERSION;
    }

    ~PngImage() {
        png_image_free(&value_);
    }

    PngImage(const PngImage&) = delete;
    PngImage& operator=(const PngImage&) = delete;

    [[nodiscard]] png_image& value() noexcept {
        return value_;
    }

  private:
    png_image value_{};
};

[[nodiscard]] Result<std::vector<std::byte>> encode_png(const ImageView& image) {
    if (image.format != PixelFormat::rgba8_unorm) {
        return Error{ErrorCode::unsupported_texture_format,
                     "glTF export supports only RGBA8 document images"};
    }
    PngImage png;
    png_image& value = png.value();
    value.width = image.width;
    value.height = image.height;
    value.format = PNG_FORMAT_RGBA;
    png_alloc_size_t size = 0;
    if (png_image_write_to_memory(&value, nullptr, &size, 0, image.pixels.data(), 0, nullptr) ==
        0) {
        return Error{ErrorCode::image_encode_failed, value.message};
    }
    std::vector<std::byte> result(static_cast<std::size_t>(size));
    if (png_image_write_to_memory(&value, result.data(), &size, 0, image.pixels.data(), 0,
                                  nullptr) == 0) {
        return Error{ErrorCode::image_encode_failed, value.message};
    }
    result.resize(static_cast<std::size_t>(size));
    return result;
}

} // namespace

Result<EncodedImageOutput> encoded_image(const ImageView& image) {
    if (image.source_mime_type != ModelImageMimeType::none) {
        const Result<image::DecodedImage> decoded = image::decode_png_or_jpeg(image.source_bytes);
        if (decoded && decoded.value().width == image.width &&
            decoded.value().height == image.height &&
            decoded.value().pixels.size() == image.pixels.size() &&
            std::equal(decoded.value().pixels.begin(), decoded.value().pixels.end(),
                       image.pixels.begin())) {
            return EncodedImageOutput{
                std::vector<std::byte>{image.source_bytes.begin(), image.source_bytes.end()},
                image.source_mime_type, false};
        }
    }
    Result<std::vector<std::byte>> encoded = encode_png(image);
    if (!encoded) {
        return encoded.error();
    }
    return EncodedImageOutput{std::move(encoded).value(), ModelImageMimeType::png, true};
}

} // namespace elf3d::gltf::exporter_detail
