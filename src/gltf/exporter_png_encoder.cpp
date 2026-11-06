#include "exporter_png_encoder.h"

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace elf3d::gltf::exporter_detail {
namespace {
constexpr std::size_t chunk_bytes = 64 * 1024;
static_assert(chunk_bytes <= std::numeric_limits<uInt>::max());
using PngTag = std::array<std::byte, 4>;
constexpr PngTag ihdr{std::byte{0x49}, std::byte{0x48}, std::byte{0x44}, std::byte{0x52}};
constexpr PngTag idat{std::byte{0x49}, std::byte{0x44}, std::byte{0x41}, std::byte{0x54}};
constexpr PngTag iend{std::byte{0x49}, std::byte{0x45}, std::byte{0x4e}, std::byte{0x44}};
constexpr PngTag srgb{std::byte{0x73}, std::byte{0x52}, std::byte{0x47}, std::byte{0x42}};
[[nodiscard]] PngTag big_endian(std::uint32_t value) noexcept
{
    return {static_cast<std::byte>((value >> 24) & 0xffU),
            static_cast<std::byte>((value >> 16) & 0xffU),
            static_cast<std::byte>((value >> 8) & 0xffU), static_cast<std::byte>(value & 0xffU)};
}
[[nodiscard]] Result<void> write_chunk(ExportBinarySpool& output, PngTag type,
                                       std::span<const std::byte> bytes)
{
    ELF3D_ASSERT(bytes.size() <= chunk_bytes);
    output.append(big_endian(static_cast<std::uint32_t>(bytes.size())));
    output.append(type);
    output.append(bytes);
    auto crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(type.data()), static_cast<uInt>(type.size()));
    if (!bytes.empty())
        crc = crc32(crc, reinterpret_cast<const Bytef*>(bytes.data()),
                    static_cast<uInt>(bytes.size()));
    // zlib's CRC32 result is representable in 32 bits on every supported platform.
    output.append(big_endian(static_cast<std::uint32_t>(crc)));
    return output.status();
}
[[nodiscard]] Result<void> write_header(std::uint32_t width, std::uint32_t height,
                                        ExportBinarySpool& output)
{
    constexpr std::array signature{std::byte{0x89}, std::byte{0x50}, std::byte{0x4e},
                                   std::byte{0x47}, std::byte{0x0d}, std::byte{0x0a},
                                   std::byte{0x1a}, std::byte{0x0a}};
    output.append(signature);
    const auto w = big_endian(width), h = big_endian(height);
    const std::array header{w[0],         w[1],         w[2],        w[3],         h[0],
                            h[1],         h[2],         h[3],        std::byte{8}, std::byte{6},
                            std::byte{0}, std::byte{0}, std::byte{0}};
    if (const auto written = write_chunk(output, ihdr, header); !written)
        return written.error();
    // The previous libpng RGBA8 encoder explicitly declared perceptual sRGB.
    constexpr std::array intent{std::byte{0}};
    return write_chunk(output, srgb, intent);
}
[[nodiscard]] bool valid_dimensions(std::uint32_t width, std::uint32_t height) noexcept
{
    constexpr auto png_maximum = 0x7fffffffU;
    if (width == 0 || height == 0 || width > png_maximum || height > png_maximum)
        return false;
    return width <= std::numeric_limits<std::size_t>::max() / 4 / height;
}
} // namespace
struct ExportPngEncoder::Impl {
    z_stream stream{};
    std::array<std::byte, chunk_bytes> input, compressed;
    std::size_t row_bytes = 0, row_offset = 0, expected = 0, accepted = 0;
    bool initialized = false, finished = false;
    ~Impl() noexcept
    {
        if (initialized)
            (void)deflateEnd(&stream);
    }
    [[nodiscard]] Result<void> initialize()
    {
        const auto code = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
        if (code == Z_MEM_ERROR)
            fatal_error("PNG encoder allocation failed");
        if (code != Z_OK)
            return Error{ErrorCode::image_encode_failed, "Cannot initialize PNG compression"};
        initialized = true;
        return {};
    }
    [[nodiscard]] Result<void> compress(std::span<const std::byte> bytes, int flush,
                                        ExportBinarySpool& output)
    {
        ELF3D_ASSERT(bytes.size() <= input.size());
        std::copy(bytes.begin(), bytes.end(), input.begin());
        stream.next_in = reinterpret_cast<Bytef*>(input.data());
        stream.avail_in = static_cast<uInt>(bytes.size());
        while (true) {
            stream.next_out = reinterpret_cast<Bytef*>(compressed.data());
            stream.avail_out = static_cast<uInt>(compressed.size());
            const auto code = deflate(&stream, flush);
            if (code == Z_MEM_ERROR)
                fatal_error("PNG compression allocation failed");
            if (code != Z_OK && code != Z_STREAM_END)
                return Error{ErrorCode::image_encode_failed, "PNG compression failed"};
            const auto count = compressed.size() - stream.avail_out;
            if (count != 0) {
                if (const auto written =
                        write_chunk(output, idat, std::span{compressed}.first(count));
                    !written)
                    return written.error();
            }
            if (code == Z_STREAM_END || (flush != Z_FINISH && stream.avail_in == 0))
                return {};
        }
    }
};
ExportPngEncoder::ExportPngEncoder() noexcept = default;
ExportPngEncoder::~ExportPngEncoder() noexcept = default;
ExportPngEncoder::ExportPngEncoder(ExportPngEncoder&&) noexcept = default;
ExportPngEncoder& ExportPngEncoder::operator=(ExportPngEncoder&&) noexcept = default;
Result<ExportPngEncoder> ExportPngEncoder::create(std::uint32_t width, std::uint32_t height,
                                                  ExportBinarySpool& output)
{
    if (!valid_dimensions(width, height))
        return Error{ErrorCode::image_encode_failed, "PNG dimensions exceed the supported range"};
    ExportPngEncoder result;
    result.impl_ = std::make_unique<Impl>();
    result.impl_->row_bytes = static_cast<std::size_t>(width) * 4;
    result.impl_->expected = result.impl_->row_bytes * height;
    if (const auto initialized = result.impl_->initialize(); !initialized)
        return initialized.error();
    if (const auto header = write_header(width, height, output); !header)
        return header.error();
    return result;
}
Result<void> ExportPngEncoder::accept(std::span<const std::byte> pixels, ExportBinarySpool& output)
{
    ELF3D_ASSERT(impl_);
    if (impl_->finished || pixels.empty() || pixels.size() > chunk_bytes ||
        pixels.size() > impl_->expected - impl_->accepted)
        return Error{ErrorCode::invalid_argument,
                     "PNG pixel packet exceeds the expected bounded stream"};
    while (!pixels.empty()) {
        if (impl_->row_offset == 0) {
            constexpr std::array filter{std::byte{0}};
            if (const auto compressed = impl_->compress(filter, Z_NO_FLUSH, output); !compressed)
                return compressed.error();
        }
        const auto count = std::min(pixels.size(), impl_->row_bytes - impl_->row_offset);
        if (const auto compressed = impl_->compress(pixels.first(count), Z_NO_FLUSH, output);
            !compressed)
            return compressed.error();
        impl_->accepted += count;
        impl_->row_offset += count;
        if (impl_->row_offset == impl_->row_bytes)
            impl_->row_offset = 0;
        pixels = pixels.subspan(count);
    }
    return {};
}
Result<void> ExportPngEncoder::finish(ExportBinarySpool& output)
{
    ELF3D_ASSERT(impl_);
    if (impl_->finished)
        return {};
    if (impl_->accepted != impl_->expected)
        return Error{ErrorCode::invalid_argument, "PNG pixel stream is incomplete"};
    if (const auto compressed = impl_->compress({}, Z_FINISH, output); !compressed)
        return compressed.error();
    const auto code = deflateEnd(&impl_->stream);
    impl_->initialized = false;
    if (code != Z_OK)
        return Error{ErrorCode::image_encode_failed, "Cannot finalize PNG compression"};
    if (const auto written = write_chunk(output, iend, {}); !written)
        return written.error();
    impl_->finished = true;
    return output.status();
}
} // namespace elf3d::gltf::exporter_detail
