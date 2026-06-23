#ifndef ELF3D_CLIPPING_H
#define ELF3D_CLIPPING_H

#include <elf3d/math/value_types.h>

#include <array>
#include <cstdint>

namespace elf3d {

inline constexpr std::uint32_t maximum_clipping_boxes = 3;

enum class PlaneHalfSpace {
    positive,
    negative,
};

struct SectionPlane {
    Float3 point{0.0F, 0.0F, 0.0F};
    Float3 normal{0.0F, 1.0F, 0.0F};
    PlaneHalfSpace retained_half_space = PlaneHalfSpace::positive;
    bool enabled = false;

    bool operator==(const SectionPlane &) const = default;
};

struct ClippingBox {
    Float3 minimum{-0.5F, -0.5F, -0.5F};
    Float3 maximum{0.5F, 0.5F, 0.5F};
    bool enabled = true;

    bool operator==(const ClippingBox &) const = default;
};

struct ClippingHelperSettings {
    bool visible = true;
    Color4 section_plane_color{0.2F, 0.75F, 1.0F, 1.0F};
    Color4 box_color{1.0F, 0.8F, 0.15F, 1.0F};
    float line_thickness_pixels = 2.0F;

    bool operator==(const ClippingHelperSettings &) const = default;
};

struct ClippingSnapshot {
    SectionPlane section_plane;
    std::array<ClippingBox, maximum_clipping_boxes> boxes;
    std::uint32_t box_count = 0;
    ClippingHelperSettings helpers;
    std::uint64_t revision = 0;

    bool operator==(const ClippingSnapshot &) const = default;
};

} // namespace elf3d

#endif
