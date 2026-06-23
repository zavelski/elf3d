#ifndef ELF3D_IMAGE_DECODER_H
#define ELF3D_IMAGE_DECODER_H

#include <elf3d/core/result.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace elf3d::image {

inline constexpr std::size_t maximum_encoded_bytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::uint32_t maximum_dimension = 16384;
inline constexpr std::size_t maximum_decoded_bytes = 256ULL * 1024ULL * 1024ULL;

struct DecodedImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Tightly packed RGBA8 rows in source order, from the image's top row to bottom row.
    std::vector<std::byte> pixels;
};

[[nodiscard]] Result<DecodedImage> decode_png_or_jpeg(std::span<const std::byte> encoded) noexcept;

} // namespace elf3d::image

#endif
