#pragma once

#include <elf3d/internal/image.h>

#include <cstddef>
#include <span>

namespace elf3d::image {

[[nodiscard]] Result<DecodedImage> decode_tiff(std::span<const std::byte> encoded);

} // namespace elf3d::image
