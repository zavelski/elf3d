#include <elf3d/core/result.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

import elf.image;

namespace {

constexpr std::array<std::uint8_t, 77> rgb_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
     0x73, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xc0,
     0x00, 0xc2, 0x0c, 0xff, 0xff, 0xff, 0x67, 0x00, 0x00, 0x1e, 0xef, 0x04, 0xfc, 0xa3, 0xc8, 0xb4,
     0xf7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

constexpr std::array<std::uint8_t, 77> rgba_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d,
     0x24, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x64, 0x64, 0x62, 0x66,
     0x01, 0x03, 0x0e, 0x0e, 0x0e, 0x0e, 0x10, 0x0d, 0x00, 0x02, 0x8e, 0x00, 0x50, 0x2c, 0xbb, 0xde,
     0x22, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

constexpr std::array<std::uint8_t, 71> gray_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
     0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x00, 0x00, 0x00, 0x00, 0x57,
     0xdd, 0x52, 0xf8, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x60,
     0x70, 0x60, 0x68, 0xf8, 0x0f, 0x00, 0x03, 0x05, 0x01, 0xc0, 0x4e, 0x33, 0x5b, 0xe9, 0x00,
     0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

std::span<const std::byte> bytes(std::span<const std::uint8_t> values) {
    return std::as_bytes(values);
}

int base64_value(char value) {
    if (value >= 'A' && value <= 'Z')
        return value - 'A';
    if (value >= 'a' && value <= 'z')
        return value - 'a' + 26;
    if (value >= '0' && value <= '9')
        return value - '0' + 52;
    return value == '+' ? 62 : value == '/' ? 63 : 0;
}

std::vector<std::byte> base64(std::string_view source) {
    std::vector<std::byte> result;
    for (std::size_t index = 0; index < source.size(); index += 4) {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(base64_value(source[index])) << 18U) |
            (static_cast<std::uint32_t>(base64_value(source[index + 1])) << 12U) |
            (static_cast<std::uint32_t>(source[index + 2] == '=' ? 0
                                                                 : base64_value(source[index + 2]))
             << 6U) |
            static_cast<std::uint32_t>(source[index + 3] == '=' ? 0
                                                                : base64_value(source[index + 3]));
        result.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
        if (source[index + 2] != '=')
            result.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
        if (source[index + 3] != '=')
            result.push_back(static_cast<std::byte>(value & 0xffU));
    }
    return result;
}

std::uint8_t pixel(const elf3d::image::DecodedImage& image, std::size_t index) {
    return std::to_integer<std::uint8_t>(image.pixels[index]);
}

std::uint32_t crc32(std::span<const std::uint8_t> values) {
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t value : values) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0 ? 0xedb88320U : 0U);
        }
    }
    return crc ^ 0xffffffffU;
}

[[nodiscard]] bool has_rgb_pixels(const elf3d::image::DecodedImage& image) {
    return pixel(image, 0) == 255 && pixel(image, 1) == 0 && pixel(image, 4) == 0 &&
           pixel(image, 5) == 255 && pixel(image, 8) == 0 && pixel(image, 10) == 255;
}

[[nodiscard]] int verify_rgb_png() {
    const auto rgb = elf3d::image::decode_png_or_jpeg(bytes(rgb_png));
    if (!rgb || rgb.value().width != 2 || rgb.value().height != 2 ||
        rgb.value().pixels.size() != 16 || !has_rgb_pixels(rgb.value())) {
        return 1;
    }
    return 0;
}

[[nodiscard]] int verify_rgba_png() {
    const auto rgba = elf3d::image::decode_png_or_jpeg(bytes(rgba_png));
    if (!rgba || pixel(rgba.value(), 0) != 1 || pixel(rgba.value(), 3) != 4 ||
        pixel(rgba.value(), 15) != 16) {
        return 2;
    }
    return 0;
}

[[nodiscard]] int verify_gray_png() {
    const auto gray = elf3d::image::decode_png_or_jpeg(bytes(gray_png));
    if (!gray || pixel(gray.value(), 0) != 0 || pixel(gray.value(), 3) != 255 ||
        pixel(gray.value(), 4) != 64 || pixel(gray.value(), 5) != 64 ||
        pixel(gray.value(), 7) != 255) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int verify_truncated_jpeg(std::span<const std::byte> jpeg) {
    const auto truncated_jpeg_result = elf3d::image::decode_png_or_jpeg(jpeg.first(32));
    if (truncated_jpeg_result ||
        truncated_jpeg_result.error().code() != elf3d::ErrorCode::image_decode_failed) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int verify_jpeg() {
    constexpr std::string_view jpeg_base64 =
        "/9j/4AAQSkZJRgABAQAAAQABAAD/"
        "2wBDAAIBAQEBAQIBAQECAgICAgQDAgICAgUEBAMEBgUGBgYFBgYGBwkIBgcJBwYGCAsICQoKCgoKBggLDAsKDAkKCg"
        "r/"
        "2wBDAQICAgICAgUDAwUKBwYHCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCg"
        "r/wAARCAACAAIDAREAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/"
        "8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJic"
        "oKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipq"
        "rKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/"
        "8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/"
        "8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRo"
        "mJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanq"
        "KmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD9B/2A/"
        "hX8MNe/YS+Cmua58ONBvb29+Enhue8vLvR4JJZ5X0u3Z5HdlJZmYkliSSSSa/"
        "yH+k9jMXkv0leNsvy+pKjh6Ob5lTp06bcKdOnDGVowhCEbRjCMUoxjFJRSSSSR6FLw+"
        "4C4hpRzXNcpw2IxWISq1atWhSqVatWp79SpUqTg5TnOTcpzk3KUm2222z//2Q==";
    const std::vector<std::byte> jpeg = base64(jpeg_base64);
    const auto decoded_jpeg = elf3d::image::decode_png_or_jpeg(jpeg);
    if (!decoded_jpeg || decoded_jpeg.value().width != 2 || decoded_jpeg.value().height != 2 ||
        pixel(decoded_jpeg.value(), 0) < 180 || pixel(decoded_jpeg.value(), 1) > 80 ||
        pixel(decoded_jpeg.value(), 2) > 80 || pixel(decoded_jpeg.value(), 3) != 255) {
        return 4;
    }
    return verify_truncated_jpeg(jpeg);
}

[[nodiscard]] int verify_invalid_images() {
    const std::array<std::byte, 3> malformed{};
    if (elf3d::image::decode_png_or_jpeg(malformed).error().code() !=
        elf3d::ErrorCode::image_decode_failed) {
        return 6;
    }
    std::array<std::uint8_t, rgb_png.size()> oversized = rgb_png;
    oversized[16] = 0x00;
    oversized[17] = 0x00;
    oversized[18] = 0x40;
    oversized[19] = 0x01;
    const std::uint32_t ihdr_crc = crc32(std::span<const std::uint8_t>{oversized}.subspan(12, 17));
    oversized[29] = static_cast<std::uint8_t>((ihdr_crc >> 24U) & 0xffU);
    oversized[30] = static_cast<std::uint8_t>((ihdr_crc >> 16U) & 0xffU);
    oversized[31] = static_cast<std::uint8_t>((ihdr_crc >> 8U) & 0xffU);
    oversized[32] = static_cast<std::uint8_t>(ihdr_crc & 0xffU);
    if (elf3d::image::decode_png_or_jpeg(bytes(oversized)).error().code() !=
        elf3d::ErrorCode::excessive_image_dimensions) {
        return 7;
    }
    return 0;
}

} // namespace

int elf3d_image_decode_test() {
    const int rgb = verify_rgb_png();
    if (rgb != 0) {
        return rgb;
    }
    const int rgba = verify_rgba_png();
    if (rgba != 0) {
        return rgba;
    }
    const int gray = verify_gray_png();
    if (gray != 0) {
        return gray;
    }
    const int jpeg = verify_jpeg();
    if (jpeg != 0) {
        return jpeg;
    }
    return verify_invalid_images();
}
