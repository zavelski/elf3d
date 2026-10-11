#include <elf3d/internal/image.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

void put(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value, std::size_t width,
         bool little)
{
    for (std::size_t index = 0U; index < width; ++index) {
        const auto shift = (little ? index : width - index - 1U) * 8U;
        bytes[offset + index] = static_cast<std::byte>((value >> shift) & 255U);
    }
}

[[nodiscard]] std::vector<std::byte> fixture(bool little, std::uint32_t bits)
{
    constexpr std::size_t directory_entries = 10U;
    constexpr std::size_t bits_offset = 8U + 2U + directory_entries * 12U + 4U;
    constexpr std::size_t offsets = bits_offset + 6U;
    constexpr std::size_t sizes = offsets + 8U;
    constexpr std::size_t pixels = sizes + 8U;
    const std::uint32_t row_bytes = 3U * (bits / 8U);
    std::vector<std::byte> bytes(pixels + row_bytes * 2U);
    bytes[0] = little ? std::byte{'I'} : std::byte{'M'};
    bytes[1] = bytes[0];
    put(bytes, 2U, 42U, 2U, little);
    put(bytes, 4U, 8U, 4U, little);
    put(bytes, 8U, directory_entries, 2U, little);
    const std::array<std::array<std::uint32_t, 4>, directory_entries> entries{{
        {256U, 4U, 1U, 1U},
        {257U, 4U, 1U, 2U},
        {258U, 3U, 3U, bits_offset},
        {259U, 3U, 1U, 1U},
        {262U, 3U, 1U, 2U},
        {273U, 4U, 2U, offsets},
        {277U, 3U, 1U, 3U},
        {278U, 4U, 1U, 1U},
        {279U, 4U, 2U, sizes},
        {284U, 3U, 1U, 1U},
    }};
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        const auto start = 10U + index * 12U;
        put(bytes, start, entry[0], 2U, little);
        put(bytes, start + 2U, entry[1], 2U, little);
        put(bytes, start + 4U, entry[2], 4U, little);
        put(bytes, start + 8U, entry[3], entry[1] == 3U && entry[2] == 1U ? 2U : 4U, little);
    }
    for (std::size_t index = 0U; index < 3U; ++index) {
        put(bytes, bits_offset + index * 2U, bits, 2U, little);
    }
    put(bytes, offsets, pixels, 4U, little);
    put(bytes, offsets + 4U, static_cast<std::uint32_t>(pixels + row_bytes), 4U, little);
    put(bytes, sizes, row_bytes, 4U, little);
    put(bytes, sizes + 4U, row_bytes, 4U, little);
    const std::array<std::uint32_t, 6> samples{255U, 0U, 128U, 0U, 255U, 64U};
    for (std::size_t index = 0U; index < samples.size(); ++index) {
        const auto sample = bits == 8U ? samples[index] : samples[index] * 257U;
        put(bytes, pixels + index * (bits / 8U), sample, bits / 8U, little);
    }
    return bytes;
}

[[nodiscard]] int verify_pixels()
{
    constexpr std::array<std::byte, 8> expected{std::byte{255}, std::byte{0},  std::byte{128},
                                                std::byte{255}, std::byte{0},  std::byte{255},
                                                std::byte{64},  std::byte{255}};
    for (const bool little : {false, true}) {
        for (const auto bits : {8U, 16U}) {
            const auto decoded = elf3d::image::decode_image(fixture(little, bits));
            if (!decoded || decoded.value().width != 1U || decoded.value().height != 2U ||
                decoded.value().pixels !=
                    std::vector<std::byte>{expected.begin(), expected.end()}) {
                return 20;
            }
        }
    }
    return 0;
}

[[nodiscard]] int verify_invalid_inputs()
{
    const auto valid = fixture(true, 16U);
    for (const std::size_t length :
         std::array<std::size_t, 5>{0U, 7U, 9U, 90U, valid.size() - 1U}) {
        if (elf3d::image::decode_image(std::span<const std::byte>{valid}.first(length))) {
            return 21;
        }
    }
    // Directory offset, unsupported compression, excessive dimensions, duplicate
    // width tag, overflowing strip offset, and insufficient strip byte count.
    const std::array<std::array<std::uint32_t, 3>, 6> mutations{{
        {4U, 0xffffffffU, 4U},
        {54U, 5U, 2U},
        {18U, 16385U, 4U},
        {22U, 256U, 2U},
        {140U, 0xffffffffU, 4U},
        {148U, 1U, 4U},
    }};
    for (const auto& mutation : mutations) {
        auto changed = valid;
        put(changed, mutation[0], mutation[1], mutation[2], true);
        if (elf3d::image::decode_image(changed)) {
            return 22;
        }
    }
    return 0;
}

} // namespace

int elf3d_tiff_decode_test()
{
    if (const int pixels = verify_pixels(); pixels != 0) {
        return pixels;
    }
    return verify_invalid_inputs();
}
