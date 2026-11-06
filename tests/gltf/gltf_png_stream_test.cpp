#include <gltf/exporter_binary_spool.hpp>
#include <gltf/exporter_output.hpp>
#include <gltf/exporter_png_encoder.h>
#include <image/image.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::gltf::exporter_detail;
struct Dimensions {
    std::uint32_t width, height;
};
[[nodiscard]] std::byte pixel_at(std::size_t offset)
{
    // Every fixture has fewer than 2^32 bytes. Unsigned hash wrapping is deliberate.
    auto value = static_cast<std::uint32_t>(offset) * 2654435761U;
    value ^= value >> 15;
    value *= 2246822519U;
    value ^= value >> 13;
    return static_cast<std::byte>(value & 0xffU);
}
[[nodiscard]] bool pixels_match(std::span<const std::byte> pixels, Dimensions size)
{
    if (pixels.size() != static_cast<std::size_t>(size.width) * size.height * 4)
        return false;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        if (pixels[i] != pixel_at(i))
            return false;
    }
    return true;
}
[[nodiscard]] bool feed(ExportPngEncoder& encoder, ExportBinarySpool& output, Dimensions size)
{
    const auto count = static_cast<std::size_t>(size.width) * size.height * 4;
    std::array<std::byte, 64 * 1024> pixels;
    for (std::size_t offset = 0; offset < count;) {
        const auto maximum = offset < 23 ? 3U : pixels.size();
        const auto chunk_count = std::min(count - offset, maximum);
        for (std::size_t i = 0; i < chunk_count; ++i)
            pixels[i] = pixel_at(offset + i);
        if (!encoder.accept(std::span{pixels}.first(chunk_count), output))
            return false;
        offset += chunk_count;
    }
    return encoder.finish(output).has_value();
}
[[nodiscard]] std::uint32_t big_endian(std::span<const std::byte> bytes)
{
    std::uint32_t result = 0;
    for (const auto value : bytes)
        result = (result << 8) | std::to_integer<std::uint32_t>(value);
    return result;
}
[[nodiscard]] bool has_srgb(std::span<const std::byte> bytes)
{
    constexpr std::array tag{std::byte{0x73}, std::byte{0x52}, std::byte{0x47}, std::byte{0x42}};
    constexpr std::array crc{std::byte{0xae}, std::byte{0xce}, std::byte{0x1c}, std::byte{0xe9}};
    for (std::size_t offset = 8; offset <= bytes.size() && bytes.size() - offset >= 12;) {
        const auto length = big_endian(bytes.subspan(offset, 4));
        if (length > bytes.size() - offset - 12)
            return false;
        if (std::equal(tag.begin(), tag.end(), bytes.subspan(offset + 4, 4).begin()))
            return length == 1 && bytes[offset + 8] == std::byte{0} &&
                   std::equal(crc.begin(), crc.end(), bytes.subspan(offset + 9, 4).begin());
        offset += std::size_t{12} + length;
    }
    return false;
}
[[nodiscard]] bool has_footer(std::span<const std::byte> bytes)
{
    constexpr std::array footer{std::byte{0},    std::byte{0},    std::byte{0},    std::byte{0},
                                std::byte{0x49}, std::byte{0x45}, std::byte{0x4e}, std::byte{0x44},
                                std::byte{0xae}, std::byte{0x42}, std::byte{0x60}, std::byte{0x82}};
    return bytes.size() >= footer.size() &&
           std::equal(footer.begin(), footer.end(), bytes.last(footer.size()).begin());
}
class PngImageOwner final {
  public:
    PngImageOwner() noexcept
    {
        value.version = PNG_IMAGE_VERSION;
    }
    ~PngImageOwner() noexcept
    {
        png_image_free(&value);
    }
    PngImageOwner(const PngImageOwner&) = delete;
    PngImageOwner& operator=(const PngImageOwner&) = delete;
    png_image value{};
};
[[nodiscard]] bool wide_decode(std::span<const std::byte> bytes, Dimensions size)
{
    PngImageOwner image;
    if (!png_image_begin_read_from_memory(&image.value, bytes.data(), bytes.size()))
        return false;
    image.value.format = PNG_FORMAT_RGBA;
    std::vector<std::byte> pixels(PNG_IMAGE_SIZE(image.value));
    if (!png_image_finish_read(&image.value, nullptr, pixels.data(), 0, nullptr))
        return false;
    return image.value.width == size.width && image.value.height == size.height &&
           pixels_match(pixels, size);
}
[[nodiscard]] bool decode_matches(std::span<const std::byte> bytes, Dimensions size)
{
    if (!has_srgb(bytes) || !has_footer(bytes))
        return false;
    if (size.width > image::maximum_dimension)
        return wide_decode(bytes, size);
    const auto decoded = image::decode_image(bytes);
    return decoded && decoded.value().width == size.width &&
           decoded.value().height == size.height && pixels_match(decoded.value().pixels, size);
}
[[nodiscard]] bool copy_matches(ExportBinarySpool& output, const std::filesystem::path& path,
                                std::span<const std::byte> expected)
{
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file || !output.copy_to(file) || !elf3d::gltf::exporter_output::close_file(file))
        return false;
    std::ifstream input{path, std::ios::binary};
    const std::vector<char> copied{std::istreambuf_iterator<char>{input}, {}};
    if (copied.size() != expected.size())
        return false;
    for (std::size_t i = 0; i < copied.size(); ++i) {
        if (std::bit_cast<std::byte>(copied[i]) != expected[i])
            return false;
    }
    return true;
}
[[nodiscard]] bool inspect_output(ExportBinarySpool& output, const std::filesystem::path& path,
                                  Dimensions size)
{
    std::array<std::byte, 17> peek;
    if (!output.read(5, peek))
        return false;
    std::vector<std::byte> bytes(output.size());
    if (!output.read(0, bytes) || !decode_matches(bytes, size))
        return false;
    if (output.read(output.size(), std::span{peek}.first(1)))
        return false;
    if (!copy_matches(output, path, bytes))
        return false;
    auto staged = output.take_path();
    if (!staged)
        return false;
    elf3d::gltf::exporter_output::OutputFiles files;
    files.adopt(path, std::move(staged).value());
    const bool published = files.publish().has_value();
    const auto discarded = files.discard();
    return published && discarded && output.discard();
}
[[nodiscard]] bool encode_case(const std::filesystem::path& directory, Dimensions size)
{
    const auto path = directory / ("stream-png-" + std::to_string(size.width) + ".png");
    ExportBinarySpool output;
    if (!output.open(path))
        return false;
    auto encoder = ExportPngEncoder::create(size.width, size.height, output);
    const bool correct =
        encoder && feed(encoder.value(), output, size) && inspect_output(output, path, size);
    const auto discarded = output.discard();
    return correct && discarded;
}
[[nodiscard]] bool packet_boundaries(ExportPngEncoder& encoder, ExportBinarySpool& output)
{
    constexpr std::array<std::byte, 9> bytes{};
    if (encoder.accept(bytes, output) || encoder.accept({}, output))
        return false;
    if (!encoder.accept(std::span{bytes}.first(8), output) || !encoder.finish(output))
        return false;
    return encoder.finish(output) && !encoder.accept(std::span{bytes}.first(1), output);
}
[[nodiscard]] bool invalid_input(const std::filesystem::path& directory)
{
    ExportBinarySpool output;
    if (!output.open(directory / "invalid-png.png"))
        return false;
    if (ExportPngEncoder::create(0, 1, output) || ExportPngEncoder::create(0x80000000U, 1, output))
        return false;
    auto encoder = ExportPngEncoder::create(2, 1, output);
    if (!encoder || encoder.value().finish(output))
        return false;
    const bool checked = packet_boundaries(encoder.value(), output);
    const auto discarded = output.discard();
    return checked && discarded;
}
} // namespace
bool elf3d_png_stream_tests(const std::filesystem::path& directory)
{
    constexpr std::array cases{Dimensions{1, 513}, Dimensions{257, 129}, Dimensions{30000, 1}};
    for (const auto size : cases) {
        if (!encode_case(directory, size)) {
            std::fprintf(stderr, "FAIL bounded PNG encoding: %u x %u\n", size.width, size.height);
            return false;
        }
    }
    return invalid_input(directory);
}
