module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>

#include <jpeglib.h>
#include <png.h>
#include <setjmp.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.image;

namespace elf3d::image {
namespace {

[[noreturn]] void fatal_image_allocation_failure() noexcept {
    fatal_error("Elf3D image decoder memory allocation failed");
}

[[noreturn]] void fatal_unexpected_image_boundary_exception() noexcept {
    fatal_error("Elf3D image decoder encountered an unexpected exception");
}

[[nodiscard]] Result<std::size_t> rgba8_size(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        return Error{ErrorCode::zero_image_dimensions,
                     "Decoded images require positive width and height"};
    }
    if (width > maximum_dimension || height > maximum_dimension) {
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
    return width_value * height_value * 4;
}

[[nodiscard]] bool has_png_signature(std::span<const std::byte> encoded) noexcept {
    constexpr std::uint8_t signature[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (encoded.size() < std::size(signature)) {
        return false;
    }
    for (std::size_t index = 0; index < std::size(signature); ++index) {
        if (std::to_integer<std::uint8_t>(encoded[index]) != signature[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool has_jpeg_signature(std::span<const std::byte> encoded) noexcept {
    return encoded.size() >= 3 && std::to_integer<std::uint8_t>(encoded[0]) == 0xffU &&
           std::to_integer<std::uint8_t>(encoded[1]) == 0xd8U &&
           std::to_integer<std::uint8_t>(encoded[2]) == 0xffU;
}

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
    PngImage(PngImage&&) = delete;
    PngImage& operator=(PngImage&&) = delete;

    [[nodiscard]] png_image& value() noexcept {
        return value_;
    }

  private:
    png_image value_{};
};

[[nodiscard]] Error png_decode_error(std::string_view prefix, const char* raw_message) {
    const std::string_view message =
        raw_message != nullptr && raw_message[0] != '\0' ? raw_message : "unknown error";
    return Error{ErrorCode::image_decode_failed, std::string{prefix} + ": " + std::string{message}};
}

[[nodiscard]] Result<DecodedImage> decode_png(std::span<const std::byte> encoded) {
    PngImage owner;
    png_image& image = owner.value();
    if (png_image_begin_read_from_memory(&image, encoded.data(), encoded.size()) == 0) {
        return png_decode_error("PNG header decoding failed", image.message);
    }

    const Result<std::size_t> decoded_size = rgba8_size(image.width, image.height);
    if (!decoded_size) {
        return decoded_size.error();
    }

    DecodedImage result;
    result.width = image.width;
    result.height = image.height;
    result.pixels.resize(decoded_size.value());
    image.format = PNG_FORMAT_RGBA;
    if (png_image_finish_read(&image, nullptr, result.pixels.data(), 0, nullptr) == 0) {
        return png_decode_error("PNG pixel decoding failed", image.message);
    }
    return result;
}

struct JpegJumpState final {
    jmp_buf jump{};
    char message[JMSG_LENGTH_MAX]{};
};

struct JpegErrorManager final {
    jpeg_error_mgr base{};
    JpegJumpState* jump_state = nullptr;
};

extern "C" void jpeg_fail(j_common_ptr decoder) {
    auto* error = reinterpret_cast<JpegErrorManager*>(decoder->err);
    decoder->err->format_message(decoder, error->jump_state->message);
    longjmp(error->jump_state->jump, 1);
}

extern "C" void jpeg_discard_message(j_common_ptr) {}

struct JpegContext final {
    JpegJumpState jump_state{};
    jpeg_decompress_struct decoder{};
    JpegErrorManager error{};
    DecodedImage image;
    std::vector<JSAMPLE> scanline;
    bool decoder_created = false;

    ~JpegContext() {
        if (decoder_created) {
            jpeg_destroy_decompress(&decoder);
        }
    }
};

#if defined(_MSC_VER)
// IJG reports fatal decode failures through longjmp. The decoder and all
// non-trivial mutable state are heap-owned before setjmp so no C++ object
// lifetime is skipped when control returns here.
#pragma warning(push)
#pragma warning(disable : 4611)
#endif
[[nodiscard]] Result<DecodedImage> decode_jpeg(std::span<const std::byte> encoded) {
    auto context = std::make_unique<JpegContext>();
    context->decoder.err = jpeg_std_error(&context->error.base);
    context->error.base.error_exit = jpeg_fail;
    context->error.base.output_message = jpeg_discard_message;
    context->error.jump_state = &context->jump_state;

    if (setjmp(context->jump_state.jump) != 0) {
        const std::string_view message =
            context->jump_state.message[0] != '\0' ? context->jump_state.message : "unknown error";
        return Error{ErrorCode::image_decode_failed,
                     "JPEG decoding failed: " + std::string{message}};
    }

    context->decoder_created = true;
    jpeg_create_decompress(&context->decoder);
    context->decoder.mem->max_memory_to_use = static_cast<long>(maximum_decoded_bytes);
    jpeg_mem_src(&context->decoder, reinterpret_cast<const unsigned char*>(encoded.data()),
                 encoded.size());
    if (jpeg_read_header(&context->decoder, TRUE) != JPEG_HEADER_OK) {
        return Error{ErrorCode::image_decode_failed, "JPEG header decoding did not complete"};
    }

    std::size_t decoded_size_value = 0;
    {
        const Result<std::size_t> decoded_size =
            rgba8_size(context->decoder.image_width, context->decoder.image_height);
        if (!decoded_size) {
            return decoded_size.error();
        }
        decoded_size_value = decoded_size.value();
    }

    context->decoder.out_color_space = JCS_RGB;
    context->image.width = context->decoder.image_width;
    context->image.height = context->decoder.image_height;
    context->image.pixels.resize(decoded_size_value);
    context->scanline.resize(static_cast<std::size_t>(context->image.width) * 3);

    if (jpeg_start_decompress(&context->decoder) == FALSE ||
        context->decoder.output_width != context->image.width ||
        context->decoder.output_height != context->image.height ||
        context->decoder.output_components != 3) {
        return Error{ErrorCode::image_decode_failed,
                     "JPEG decoder did not produce the requested RGB output"};
    }

    while (context->decoder.output_scanline < context->decoder.output_height) {
        JSAMPROW row = context->scanline.data();
        if (jpeg_read_scanlines(&context->decoder, &row, 1) != 1) {
            return Error{ErrorCode::image_decode_failed, "JPEG scanline decoding did not complete"};
        }
        const std::size_t row_index =
            static_cast<std::size_t>(context->decoder.output_scanline - 1);
        const std::size_t destination_row =
            row_index * static_cast<std::size_t>(context->image.width) * 4;
        for (std::size_t column = 0; column < context->image.width; ++column) {
            const std::size_t source = column * 3;
            const std::size_t destination = destination_row + column * 4;
            context->image.pixels[destination] = static_cast<std::byte>(context->scanline[source]);
            context->image.pixels[destination + 1] =
                static_cast<std::byte>(context->scanline[source + 1]);
            context->image.pixels[destination + 2] =
                static_cast<std::byte>(context->scanline[source + 2]);
            context->image.pixels[destination + 3] = static_cast<std::byte>(0xffU);
        }
    }

    if (jpeg_finish_decompress(&context->decoder) == FALSE) {
        return Error{ErrorCode::image_decode_failed, "JPEG pixel decoding did not complete"};
    }
    jpeg_destroy_decompress(&context->decoder);
    context->decoder_created = false;
    return std::move(context->image);
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace

Result<DecodedImage> decode_png_or_jpeg(std::span<const std::byte> encoded) noexcept {
    if (encoded.empty()) {
        return Error{ErrorCode::image_decode_failed, "Image decoding requires encoded bytes"};
    }
    if (encoded.size() > maximum_encoded_bytes) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Encoded image exceeds the 64 MiB decoder limit"};
    }

    try {
        if (has_png_signature(encoded)) {
            return decode_png(encoded);
        }
        if (has_jpeg_signature(encoded)) {
            return decode_jpeg(encoded);
        }
        return Error{ErrorCode::image_decode_failed,
                     "Encoded image is not a supported PNG or JPEG stream"};
    } catch (const std::bad_alloc&) {
        fatal_image_allocation_failure();
    } catch (...) {
        fatal_unexpected_image_boundary_exception();
    }
}

} // namespace elf3d::image
