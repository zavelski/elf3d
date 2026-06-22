#ifndef ELF3D_MATH_VALUE_TYPES_H
#define ELF3D_MATH_VALUE_TYPES_H

#include <cstdint>

namespace elf3d {

struct Float2 {
    float x = 0.0F;
    float y = 0.0F;

    bool operator==(const Float2 &) const = default;
};

struct Color4 {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    float alpha = 1.0F;

    bool operator==(const Color4 &) const = default;
};

struct Extent2D {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool operator==(const Extent2D &) const = default;
};

} // namespace elf3d

#endif
