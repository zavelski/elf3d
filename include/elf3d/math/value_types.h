#ifndef ELF3D_MATH_VALUE_TYPES_H
#define ELF3D_MATH_VALUE_TYPES_H

#include <array>
#include <cstdint>

namespace elf3d {

struct Float2 {
    float x = 0.0F;
    float y = 0.0F;

    bool operator==(const Float2 &) const = default;
};

struct Float3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    bool operator==(const Float3 &) const = default;
};

struct Quaternion {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;

    bool operator==(const Quaternion &) const = default;
};

struct Transform {
    Float3 translation;
    Quaternion rotation;
    Float3 scale{1.0F, 1.0F, 1.0F};

    bool operator==(const Transform &) const = default;
};

// Column-major 4x4 matrix used with column vectors. The default is identity.
struct Float4x4 {
    std::array<float, 16> elements{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F,
                                   0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    bool operator==(const Float4x4 &) const = default;
};

struct Bounds3 {
    Float3 minimum;
    Float3 maximum;
    bool is_valid = false;

    bool operator==(const Bounds3 &) const = default;
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
